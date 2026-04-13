//
//  opc-parser
//
//  Created by miyako on 2025/08/29 as CLI
//  Converted by miyako to function on 2026/03/31
//

#include "opc-parser.h"

// The ZIP magic number: 50 4B 03 04
const std::array<opc_uint8_t, 4> ZIP_MAGIC = {0x50, 0x4B, 0x03, 0x04};

// Maximum bytes to scan for the magic number
const size_t MAX_SCAN_OFFSET = 512;

static bool sanitize_docx_buffer(std::vector<opc_uint8_t>& data) {
    
    // 1. Check the buffer size (must be at least 4 bytes to contain the magic number)
    if (data.size() < ZIP_MAGIC.size()) {
        return false;
    }

    // Determine the safe upper bound for our search.
    // We allow the magic number to START anywhere up to MAX_SCAN_OFFSET.
    size_t search_end_idx = std::min(data.size(), MAX_SCAN_OFFSET + ZIP_MAGIC.size());
    auto search_end_iter = data.begin() + search_end_idx;

    // 2 & 4. Check magic number / scan the first 512 bytes
    auto it = std::search(
        data.begin(),
        search_end_iter,
        ZIP_MAGIC.begin(),
        ZIP_MAGIC.end()
    );

    // If the iterator reached our search boundary, the magic number wasn't found
    if (it == search_end_iter) {
        return false;
    }

    // 3 & 5. If found at the very beginning, skip. Otherwise, remove padding.
    if (it != data.begin()) {
        // Calculate how many bytes of garbage we are removing (for logging purposes)
        size_t padding_size = std::distance(data.begin(), it);
        std::cout << "Found " << padding_size << " bytes of padding. Removing...\n";
        
        // Remove data up to before the magic number
        data.erase(data.begin(), it);
    }

    // Buffer is now guaranteed to start with 50 4B 03 04
    return true;
}

static void extract_text_nodes(xmlNode *node, std::string& text) {
    
    for (xmlNode *cur = node; cur; cur = cur->next) {
        if (
            (cur->type == XML_ELEMENT_NODE)
            &&
              ((!xmlStrcmp(cur->name, (const xmlChar *)"t"))
            || (!xmlStrcmp(cur->name, (const xmlChar *)"a:t")))
            ) {
            xmlChar *content = xmlNodeGetContent(cur);
            if (content) {
                text += (char *)content;
                xmlFree(content);
            }
        }
        extract_text_nodes(cur->children, text);
    }
}

namespace opc {
    struct Run {
        std::string text;
    };

    struct Paragraph {
        std::vector<Run> runs;
    };

    struct Page {
        std::vector<Paragraph> paragraphs;
    };

    struct Document {
        std::string type;
        std::vector<Page> pages;
    };

    struct Row {
        std::vector<std::string> cells;
    };

    struct Sheet {
        std::string name;
        std::vector<Row> rows;
    };

    struct Workbook {
        std::string type;
        std::vector<Sheet> sheets;
    };
}

using namespace opc;

static void ob_append_s(PA_CollectionRef c, const std::string& value) {
    
    C_TEXT t;
    t.setUTF8String((const uint8_t *)value.c_str(), (uint32_t)value.length());
    PA_Variable v = PA_CreateVariable(eVK_Unistring);
    PA_Unistring u = PA_CreateUnistring((PA_Unichar *)t.getUTF16StringPtr());
    PA_SetStringVariable(&v, &u);
    PA_SetCollectionElement(c, PA_GetCollectionLength(c), v);
    PA_ClearVariable(&v);
}

static void ob_append_o(PA_CollectionRef c, PA_ObjectRef value) {

    PA_Variable v = PA_CreateVariable(eVK_Object);
    PA_SetObjectVariable(&v, value);
    PA_SetCollectionElement(c, PA_GetCollectionLength(c), v);
    PA_ClearVariable(&v);
}

static void ob_append_c(PA_CollectionRef c, PA_CollectionRef value) {

    PA_Variable v = PA_CreateVariable(eVK_Collection);
    PA_SetCollectionVariable(&v, value);
    PA_SetCollectionElement(c, PA_GetCollectionLength(c), v);
    PA_ClearVariable(&v);
}

static void document_to_json_ss(Workbook& document,
                                PA_ObjectRef documentNode,
                                output_type mode,
                                int max_paragraph_length,
                                bool unique_values_only,
                                bool text_as_tokens,
                                int tokens_length,
                                bool token_padding,
                                int pooling_mode,
                                float overlap_ratio) {
    
    switch (mode) {
        case output_type_text:
        {
            PA_CollectionRef pages = PA_CreateCollection();
            for (const auto &sheet : document.sheets) {
                bool emptyCol = true;
                std::string paragraphs;
                for (const auto &row : sheet.rows) {
                    bool emptyRow = true;
                    std::unordered_set<std::string> seen_value;
                    std::unordered_set<std::string> seen;
                    int paragraph_length = 0;
                    std::string joined;
                    for (const auto &cell : row.cells) {
                        if(cell.empty())
                            continue;
                        
                        if (!joined.empty()) joined += " ";
                        joined += cell;
                        emptyRow = false;
                        emptyCol = false;
                        
                        if ((unique_values_only) && (!seen_value.insert(cell).second)) {
                            continue;
                        }
                        
                        if(!joined.empty()) joined += " ";
                        
                        joined += cell;
                    }
                    
                    if(emptyRow) {
                        continue;
                    }
                    
//                    if (!text.empty()) text += "\n";
//                    text += joined.c_str();
                    
                    if ((unique_values_only) && (!seen.insert(joined).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    if(!paragraphs.empty()) paragraphs += "\n";
                    paragraphs += joined;
                    
                    if (max_paragraph_length > 0) {
                        paragraph_length++;
                        if (paragraph_length == max_paragraph_length) {
                            ob_append_s(pages, paragraphs);
                            paragraph_length = 0;
                            emptyCol = true;
                            paragraphs.clear();
                            continue;
                        }
                    }
                }
                if (emptyCol)
                    continue;
                
                ob_append_s(pages, paragraphs);
            }
//            PA_CollectionRef matrix = process_paragraph(text, tokens_length, token_padding, !text_as_tokens, overlap_ratio, pooling_mode);
//            ob_set_c(documentNode, "input", matrix);
            ob_set_c(documentNode, L"documents", pages);
        }
            break;
        case output_type_collection:
        {
            ob_set_s(documentNode, "type", document.type.c_str());
            std::vector<std::string> texts;
            for (const auto &sheet : document.sheets) {
                bool emptyCol = true;
                PA_ObjectRef sheetNode = PA_CreateObject();
                PA_ObjectRef sheetMetaNode = PA_CreateObject();
                ob_set_s(sheetMetaNode, "name", sheet.name.c_str());
                ob_set_o(sheetNode, "meta", sheetMetaNode);
                std::string paragraphs;
                for (const auto &row : sheet.rows) {
                    PA_CollectionRef values = PA_CreateCollection();
                    std::unordered_set<std::string> seen_value;
                    std::unordered_set<std::string> seen;
                    int paragraph_length = 0;
                    std::string joined;
                    for (const auto &cell : row.cells) {
                        if(cell.empty())
                            continue;

                        if ((unique_values_only) && (!seen_value.insert(cell).second)) {
                            continue;
                        }
                        
                        if(!joined.empty()) joined += " ";
                        
                        joined += cell;
                        ob_append_s(values, cell);
                    }
                    
                    if(joined.empty())
                        continue;
                    
                    if ((unique_values_only) && (!seen.insert(joined).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    if(!paragraphs.empty()) paragraphs += "\n";
                    paragraphs += joined;
                    
                    if (max_paragraph_length > 0) {
                        paragraph_length++;
                        if (paragraph_length == max_paragraph_length) {
                            texts.push_back(paragraphs);
                            paragraph_length = 0;
                            emptyCol = true;
                            paragraphs.clear();
                            continue;
                        }
                    }
                }
                if (emptyCol) {
                    //empty page
                    continue;
                }
                texts.push_back(paragraphs);
            }
            PA_CollectionRef matrix = process_paragraphs(texts, tokens_length, token_padding, !text_as_tokens, overlap_ratio, pooling_mode);
            ob_set_c(documentNode, "input", matrix);
        }
            break;
        case output_type_collections:
        {
            ob_set_s(documentNode, "type", document.type.c_str());
            PA_CollectionRef pages = PA_CreateCollection();
            std::vector<std::string> texts;
            for (const auto &sheet : document.sheets) {
                bool emptyCol = true;
                PA_ObjectRef sheetNode = PA_CreateObject();
                PA_ObjectRef sheetMetaNode = PA_CreateObject();
                ob_set_s(sheetMetaNode, "name", sheet.name.c_str());
                ob_set_o(sheetNode, "meta", sheetMetaNode);
//                PA_CollectionRef paragraphs = PA_CreateCollection();
                for (const auto &row : sheet.rows) {
                    PA_CollectionRef values = PA_CreateCollection();
                    std::unordered_set<std::string> seen_value;
                    std::unordered_set<std::string> seen;
                    int paragraph_length = 0;
                    std::string joined;
                    for (const auto &cell : row.cells) {
                        if(cell.empty())
                            continue;

                        if ((unique_values_only) && (!seen_value.insert(cell).second)) {
                            continue;
                        }
                        
                        if(!joined.empty()) joined += " ";
                        
                        joined += cell;
                        ob_append_s(values, cell);
                    }
                    
                    if(joined.empty())
                        continue;
                    
                    if ((unique_values_only) && (!seen.insert(joined).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    texts.push_back(joined);
                    
                    if (max_paragraph_length > 0) {
                        paragraph_length++;
                        if (paragraph_length == max_paragraph_length) {
                            if (text_as_tokens) {
                                PA_CollectionRef matrix = process_paragraphs(texts, tokens_length, token_padding, false, overlap_ratio, pooling_mode);
                                ob_append_c(pages, matrix);
                            } else {
                                PA_CollectionRef matrix = process_paragraphs(texts, tokens_length, token_padding, true, overlap_ratio, pooling_mode);
                                ob_append_c(pages, matrix);
                            }
                            paragraph_length = 0;
                            emptyCol = true;
//                            paragraphs = PA_CreateCollection();
                            continue;
                        }
                    }
                }
                if (emptyCol) {
                    //empty page
                    continue;
                }
                PA_CollectionRef matrix = process_paragraphs(texts, tokens_length, token_padding, !text_as_tokens, overlap_ratio, pooling_mode);
                ob_append_c(pages, matrix);
            }
            ob_set_c(documentNode, L"inputs", pages);
        }
            break;
        case output_type_object:
        default:
        {
            ob_set_s(documentNode, "type", document.type.c_str());
            PA_CollectionRef pages = PA_CreateCollection();
            int colIdx = 0;
            for (const auto &sheet : document.sheets) {
                bool emptyCol = true;
                PA_ObjectRef sheetNode = PA_CreateObject();
                PA_ObjectRef sheetMetaNode = PA_CreateObject();
                ob_set_s(sheetMetaNode, "name", sheet.name.c_str());
                ob_set_o(sheetNode, "meta", sheetMetaNode);
                PA_CollectionRef paragraphs = PA_CreateCollection();
                int rowIdx = 0; // physical sheet row index, increments regardless of empty rows
                for (const auto &row : sheet.rows) {
                    PA_ObjectRef paragraphNode = PA_CreateObject();
                    PA_CollectionRef values = PA_CreateCollection();
                    bool emptyRow = true;
                    std::string joined;
                    int paragraph_length = 0;
                    std::unordered_set<std::string> seen;
                    for (const auto &cell : row.cells) {
                        if(cell.empty())
                            continue;
                        
                        if ((unique_values_only) && (!seen.insert(cell).second)) {
//                            std::cerr << "skip duplicate value:" << cell << std::endl;
                            continue;
                        }
                        
                        if (!joined.empty()) joined += " ";
                        joined += cell;
                        emptyRow = false;
                        emptyCol = false;
                        ob_append_s(values, cell);
                        
                        if (max_paragraph_length > 0) {
                            paragraph_length++;
                            if (paragraph_length == max_paragraph_length) {
//                                std::cerr << "abort paragraph at length:" << paragraph_length << std::endl;
                                goto row_parsed_object;
                            }
                        }
                    }
                    if(emptyRow) {
                        rowIdx++;
                        continue;
                    }
                row_parsed_object:
                    ob_set_c(paragraphNode, "values", values);
                    ob_set_n(paragraphNode, "index", rowIdx++);
                    ob_set_s(paragraphNode, "text", joined.c_str());
                    ob_append_o(paragraphs, paragraphNode);
                }
                if (emptyCol) {
                    colIdx++;
                    continue;
                }
                ob_set_n(sheetNode, "index", colIdx++);
                ob_set_c(sheetNode, "paragraphs", paragraphs);
                ob_append_o(pages, sheetNode);
            }
            ob_set_c(documentNode, L"pages", pages);
        }
            break;
    }
}

static void document_to_json(Document& document,
                             PA_ObjectRef documentNode,
                             output_type mode,
                             int max_paragraph_length,
                             bool unique_values_only,
                             bool text_as_tokens,
                             int tokens_length,
                             bool token_padding,
                             int pooling_mode,
                             float overlap_ratio) {

    switch (mode) {
        case output_type_text:
        {
//            std::string text = "";
            PA_CollectionRef pages = PA_CreateCollection();
            for (const auto &page : document.pages) {
                bool emptyCol = true;
                std::string paragraphs;
                std::unordered_set<std::string> seen;
                int paragraph_length = 0;
                for (const auto &paragraph : page.paragraphs) {
                    std::string joined;
                    for (const auto &run : paragraph.runs) {
                        if(run.text.empty())
                            continue;
                        
                        joined += run.text;
                    }
                    
                    if(joined.empty())
                        continue;
                    
                    emptyCol = false;
                    
//                    if (!text.empty()) text += "\n";
//                    text += joined.c_str();
                    
                    if ((unique_values_only) && (!seen.insert(joined).second)) {
                        continue;
                    }
                    
                    if(!paragraphs.empty()) paragraphs += "\n";
                    paragraphs += joined;
                    
                    if (max_paragraph_length > 0) {
                        paragraph_length++;
                        if (paragraph_length == max_paragraph_length) {
                            ob_append_s(pages, paragraphs);
                            paragraph_length = 0;
                            emptyCol = true;
                            paragraphs.clear();
                            continue;
                        }
                    }
                }
                if (emptyCol) {
                    //empty page
                    continue;
                }
                ob_append_s(pages, paragraphs);
            }
//            PA_CollectionRef matrix = process_paragraph(text, tokens_length, token_padding, !text_as_tokens, overlap_ratio, pooling_mode);
//            ob_set_c(documentNode, "input", matrix);
            ob_set_c(documentNode, L"documents", pages);
        }
            break;
        case output_type_collection:
        {
            ob_set_s(documentNode, "type", document.type.c_str());
            std::vector<std::string> texts;
            for (const auto &page : document.pages) {
                bool emptyCol = true;
                std::string paragraphs;
                //no need for colIdx, rowIdx
                std::unordered_set<std::string> seen;
                int paragraph_length = 0;
                for (const auto &paragraph : page.paragraphs) {
                    std::string joined;
                    for (const auto &run : paragraph.runs) {
                        if(run.text.empty())
                            continue;
                        
                        joined += run.text;
                    }
                    
                    if(joined.empty())
                        continue;
                    
                    if ((unique_values_only) && (!seen.insert(joined).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    if(!paragraphs.empty()) paragraphs += "\n";
                    paragraphs += joined;
                    
                    if (max_paragraph_length > 0) {
                        paragraph_length++;
                        if (paragraph_length == max_paragraph_length) {
                            texts.push_back(paragraphs);
                            paragraph_length = 0;
                            emptyCol = true;
                            paragraphs.clear();
                            continue;
                        }
                    }
                }
                if (emptyCol) {
                    //empty page
                    continue;
                }
                texts.push_back(paragraphs);
            }
            PA_CollectionRef matrix = process_paragraphs(texts, tokens_length, token_padding, !text_as_tokens, overlap_ratio, pooling_mode);
            ob_set_c(documentNode, "input", matrix);
        }
            break;
        case output_type_collections:
        {
            ob_set_s(documentNode, "type", document.type.c_str());
            PA_CollectionRef pages = PA_CreateCollection();
            std::vector<std::string> texts;
            for (const auto &page : document.pages) {
                bool emptyCol = true;
                std::unordered_set<std::string> seen;
                int paragraph_length = 0;
                for (const auto &paragraph : page.paragraphs) {
                    std::string joined;
                    for (const auto &run : paragraph.runs) {
                        if(run.text.empty())
                            continue;
                        
                        joined += run.text;
                    }
                    
                    if(joined.empty())
                        continue;
                    
                    if ((unique_values_only) && (!seen.insert(joined).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    texts.push_back(joined);
                    
                    if (max_paragraph_length > 0) {
                        paragraph_length++;
                        if (paragraph_length == max_paragraph_length) {
                            PA_CollectionRef matrix = process_paragraphs(texts, tokens_length, token_padding, !text_as_tokens, overlap_ratio, pooling_mode);
                            ob_append_c(pages, matrix);
                            paragraph_length = 0;
                            emptyCol = true;
                            texts.clear();
                            continue;
                        }
                    }
                }
                if (emptyCol) {
                    //empty page
                    continue;
                }
                PA_CollectionRef matrix = process_paragraphs(texts, tokens_length, token_padding, !text_as_tokens, overlap_ratio, pooling_mode);
                ob_append_c(pages, matrix);
                texts.clear();
            }
            ob_set_c(documentNode, L"inputs", pages);
        }
            break;
        case output_type_object:
        default:
        {
            //ignore max_paragraph_length, unique_values_only
            ob_set_s(documentNode, "type", document.type.c_str());
            PA_CollectionRef pages = PA_CreateCollection();
            int colIdx = 0;
            for (const auto &page : document.pages) {
                bool emptyCol = true;
                PA_ObjectRef pageNode = PA_CreateObject();
                PA_CollectionRef paragraphs = PA_CreateCollection();
                int rowIdx = 0; // physical sheet row index, increments regardless of empty rows
                for (const auto &paragraph : page.paragraphs) {
                    PA_ObjectRef paragraphNode = PA_CreateObject();
                    std::string joined;
                    for (const auto &run : paragraph.runs) {
                        if(run.text.empty())
                            continue;
                        
                        joined += run.text;
                    }
                    
                    if(joined.empty())
                        continue;
                    
                    emptyCol = false;
                                        
                    ob_set_n(paragraphNode, "index", rowIdx++);
                    ob_set_s(paragraphNode, "text", joined.c_str());
                    ob_append_o(paragraphs, paragraphNode);
                }
                if (emptyCol) {
                    //empty page
                    colIdx++;
                    continue;
                }
                ob_set_c(pageNode, "paragraphs", paragraphs);
                ob_set_n(pageNode, "index", colIdx++);
                ob_append_o(pages, pageNode);
            }
            ob_set_c(documentNode, L"pages", pages);
        }
            break;
    }
}

static std::string node_text(xmlNodePtr node) {
                            
    if (!node) return "";
    xmlChar* t = xmlNodeGetContent(node);
    if (!t) return "";
    std::string s = (const char*)t;
    xmlFree(t);
                            
    return s;
}

static xmlNodePtr find_first_child(xmlNodePtr parent, const char* localname) {
                            
    for (xmlNodePtr c = parent ? parent->children : nullptr; c; c = c->next) {
        if (c->type == XML_ELEMENT_NODE && strcmp((const char*)c->name, localname) == 0) return c;
    }
    return nullptr;
}

static std::string xml_get_text_concat(xmlNodePtr node, const char* elem_localname) {
                            
    // Concatenate all descendant <elem_localname> text nodes (e.g., <t> inside rich text)
    std::string out;
    std::vector<xmlNodePtr> stack;
    if (node) stack.push_back(node);
    while (!stack.empty()) {
        xmlNodePtr cur = stack.back(); stack.pop_back();
        if (cur->type == XML_ELEMENT_NODE && strcmp((const char*)cur->name, elem_localname) == 0) {
            xmlChar* txt = xmlNodeGetContent(cur);
            if (txt) { out.append((const char*)txt); xmlFree(txt); }
        }
        for (xmlNodePtr c = cur->children; c; c = c->next) stack.push_back(c);
    }
    return out;
}

static std::string resolve_inline_str(xmlNodePtr c_node) {
                            
    // inlineStr → child <is> rich text with one or more <t>
    xmlNodePtr is = find_first_child(c_node, "is");
    if (!is) return "";
    return xml_get_text_concat(is, "t");
}

static std::string resolve_cell_value(xmlNodePtr c_node, const std::vector<std::string>& sst) {
    // <c t="s|inlineStr|str|b|e|n"><v>…</v> or <is>…</is></c>
    std::string t_attr;
    if (xmlChar* t = xmlGetProp(c_node, (const xmlChar*)"t")) {
        t_attr = (const char*)t; xmlFree(t);
    }

    if (t_attr == "inlineStr") {
        return resolve_inline_str(c_node);
    }

    // Find <v>
    xmlNodePtr v = find_first_child(c_node, "v");
    std::string vtext = node_text(v);

    if (t_attr == "s") {
        // shared string index
        if (!vtext.empty()) {
            long idx = strtol(vtext.c_str(), nullptr, 10);
            if (idx >= 0 && idx < (long)sst.size()) return sst[(size_t)idx];
        }
        return "";
    }
    if (t_attr == "str") {
        return vtext; // formula string result
    }
    if (t_attr == "b") {
        // boolean 0/1
        return (vtext == "1") ? "TRUE" : "FALSE";
    }
    if (t_attr == "e") {
        // Excel error code (e.g., #DIV/0!)
        return vtext;
    }
    // default numeric (or text if the cell is styled as text; formatting not applied here)
    return vtext;
}

static void process_worksheet(xmlNode *node, Workbook& document,const char *tag, const std::vector<std::string>& sst) {
    
    Sheet *sheet = &document.sheets.back();
    
    xmlNodePtr sheetData = nullptr;
    // find sheetData anywhere under root (namespaces don't matter for local names)
    std::vector<xmlNodePtr> stack;
    if (node) stack.push_back(node);
    while (!stack.empty() && !sheetData) {
        xmlNodePtr cur = stack.back(); stack.pop_back();
        if (cur->type == XML_ELEMENT_NODE && strcmp((const char*)cur->name, "sheetData") == 0) {
            sheetData = cur; break;
        }
        for (xmlNodePtr c = cur->children; c; c = c->next) stack.push_back(c);
    }
    
    if(sheetData){
        for (xmlNodePtr row = sheetData->children; row; row = row->next) {
            if(row->type == XML_ELEMENT_NODE){
                if (!xmlStrcmp(row->name, (const xmlChar *)tag)) {
                    Row _row;
                    std::string value;
                    for (xmlNodePtr cell = row->children; cell; cell = cell->next) {
                        if (cell->type != XML_ELEMENT_NODE || strcmp((const char*)cell->name, "c") != 0) continue;
                        std::string cell_value = resolve_cell_value(cell, sst);
                        _row.cells.push_back(cell_value);
                    }
                    sheet->rows.push_back(_row);
                }
            }
        }
    }
}

static void process_document(xmlNode *node, Document& document,const char *tag, document_type mode) {
    
    Page *page = &document.pages.back();
    
    for (xmlNode *cur = node; cur; cur = cur->next) {
        if(cur->type == XML_ELEMENT_NODE){

            if(mode == document_type_docx) {
                if (!xmlStrcmp(cur->name, (const xmlChar *)"br")) {
                    xmlChar* type_attr = xmlGetProp(cur, (const xmlChar*)"type");
                    if (type_attr) {
                        if (!xmlStrcmp(type_attr, (const xmlChar *)"page")) {
                            Page _page;
                            document.pages.push_back(_page);
                        }
                        xmlFree(type_attr);
                    }
                }
                if (!xmlStrcmp(cur->name, (const xmlChar *)"pPr")) {
                    xmlNodePtr pPr = cur;
                    for(xmlNodePtr sp = pPr->children; sp; sp = sp->next) {
                        if(!xmlStrcmp(sp->name,(const xmlChar*)"sectPr")) {
                            for(xmlNodePtr t = sp->children; t; t = t->next){
                                if(!xmlStrcmp(t->name,(const xmlChar*)"type")){
                                    xmlNodePtr typeNode = t;
                                    xmlChar* val = xmlGetProp(typeNode, (const xmlChar*)"val");
                                    if(val){
                                        if(!xmlStrcmp(val,(const xmlChar*)"nextPage")){
                                            Page _page;
                                            document.pages.push_back(_page);
                                        }
                                        xmlFree(val);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!xmlStrcmp(cur->name, (const xmlChar *)tag)) {
                page = &document.pages.back();
                Paragraph _paragraph;
                page->paragraphs.push_back(_paragraph);
            }

            if((mode == document_type_docx) || (mode == document_type_pptx)){
                if((!xmlStrcmp(cur->name, (const xmlChar *)"t"))
                   || (!xmlStrcmp(cur->name, (const xmlChar *)"a:t"))
                   ) {
                    xmlChar *content = xmlNodeGetContent(cur);
                    if (content) {
                        std::string value((const char *)content);
                        if(value.length() != 0) {
                            if(!page->paragraphs.size()) {
                                //should not happen
                                Paragraph _paragraph;
                                page->paragraphs.push_back(_paragraph);
                            }
                            Paragraph *paragraph = &page->paragraphs.back();
                            paragraph->runs.push_back(Run{value});
                        }
                        xmlFree(content);
                    }
                }
            }
        }
        
        if(cur->children) {
            process_document(cur->children, document, tag, mode);
        }
    }
}

static xmlDoc *parse_opc_part(opcContainer *container, opcPart part) {
    
    if(part) {
        std::vector<opc_uint8_t>data;
        data.reserve(BUFLEN);
        std::vector<opc_uint8_t>buf(BUFLEN);
        opcContainerInputStream *stream = opcContainerOpenInputStream(container, part);
        if (stream) {
            opc_uint32_t len = 0;
            while ((len = opcContainerReadInputStream(stream, &buf[0], BUFLEN)) && (len)) {
                data.insert(data.end(), buf.begin(), buf.begin() + len);
            }
            data.push_back('\0');
            opcContainerCloseInputStream(stream);
            return xmlParseMemory((const char*)&data[0], (int)data.size());
        }
    }
    
    return NULL;
}

#ifdef _WIN32
static std::string wchar_to_utf8(const wchar_t* wstr) {
    if (!wstr) return std::string();

    // Get required buffer size in bytes
    int size_needed = WideCharToMultiByte(
        CP_UTF8,            // convert to UTF-8
        0,                  // default flags
        wstr,               // source wide string
        -1,                 // null-terminated
        nullptr, 0,         // no output buffer yet
        nullptr, nullptr
    );

    if (size_needed <= 0) return std::string();

    // Allocate buffer
    std::string utf8str(size_needed, 0);

    // Perform conversion
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr,
        -1,
        &utf8str[0],
        size_needed,
        nullptr,
        nullptr
    );

    // Remove the extra null terminator added by WideCharToMultiByte
    if (!utf8str.empty() && utf8str.back() == '\0') {
        utf8str.pop_back();
    }

    return utf8str;
}
#endif

bool opc_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                    output_type mode,
                    int max_paragraph_length,
                    bool unique_values_only,
                    bool text_as_tokens,
                    int tokens_length,
                    bool token_padding,
                    int pooling_mode,
                    float overlap_ratio,
                    std::string password) {
        
    if(password.length()) {

        ms::Format format;
        try {
            format = ms::DetectFormat((const char *)data.data(),
                                      (size_t)data.size());
        } catch (std::exception& e) {
            format = ms::fUnknown;
        }
        if (format != ms::fZip) {
            
            cybozu::String16 wpass;
            std::string passData;
            if (cybozu::ConvertUtf8ToUtf16(&wpass, password)) {
                passData = ms::Char16toChar8(wpass);
            }
            
            std::string decData;
            std::string secretKey;
            ms::cfb::CompoundFile cfb((const char *)data.data(), (uint32_t)data.size());
            cfb.put();

            const std::string& encryptedPackage = ms::GetContensByName(cfb, "EncryptedPackage"); // data
            const ms::EncryptionInfo info(ms::GetContensByName(cfb, "EncryptionInfo")); // xml
            info.put();

            bool decoded = false;
            if (info.isStandardEncryption) {
                decoded = ms::decodeStandardEncryption(decData,
                                                       encryptedPackage,
                                                       info, passData, secretKey);
            } else {
                decoded = ms::decodeAgile(decData,
                                          encryptedPackage,
                                          info, passData, secretKey);
            }
            
            if (decoded) {
                data.assign(decData.begin(), decData.end());
            }else{
                std::cerr << "incorrect password!" << std::endl;
                return false;
            }
        }
    }

    document_type type = document_type_unknown;
    Document document;
    Workbook workbook;
    opcPart part = OPC_PART_INVALID;
    opcContainer *container = NULL;
    opcRelation rel = OPC_RELATION_INVALID;

    if(!sanitize_docx_buffer(data)) {
        std::cerr << "not a valid input!" << std::endl;
        goto unfortunately;
    }
    
    container = opcContainerOpenMem(_X(data.data()), (opc_uint32_t)data.size(), OPC_OPEN_READ_ONLY, NULL);
    
    if(!container) {
        std::cerr << "not a valid input!" << std::endl;
        goto unfortunately;
    }
        
    part = opcPartFind(container, _X("/word/document.xml"), NULL, 0);
    if(part) {
        document.type = "docx";
        type = document_type_docx;
        goto reader;
    }
    part = opcPartFind(container, _X("/xl/workbook.xml"), NULL, 0);
    if(part) {
        workbook.type = "xlsx";
        type = document_type_xlsx;
        goto reader;
    }
    part = opcPartFind(container, _X("/ppt/presentation.xml"), NULL, 0);
    if(part) {
        document.type = "pptx";
        type = document_type_pptx;
        goto reader;
    }
    
    rel = opcRelationFind(
        container,
        OPC_PART_INVALID,
        NULL,
        _X("http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument")
    );

    if (OPC_RELATION_INVALID != rel) {
        part = opcRelationGetInternalTarget(container, OPC_PART_INVALID, rel);
           if (OPC_PART_INVALID != part) {
               const xmlChar *ct = opcPartGetType(container, part);
               if (0 == xmlStrcmp(ct, _X("application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"))) {
                   document.type = "docx";
                   type = document_type_docx;
               } else if (0 == xmlStrcmp(ct, _X("application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"))) {
                   document.type = "pptx";
                   type = document_type_pptx;
               } else if (0 == xmlStrcmp(ct, _X("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"))) {
                   document.type = "xlsx";
                   type = document_type_xlsx;
               }
               goto reader;
           }
    }
    
    reader:
    
    if(part) {
        switch (type) {
            case document_type_docx:
            {
                xmlDoc *xml_doc = parse_opc_part(container, part);
                if (xml_doc) {
                    xmlNode *doc_root = xmlDocGetRootElement(xml_doc);
                    if(doc_root) {
                        Page _page;
                        document.pages.push_back(_page);
                        process_document(doc_root, document, "p", type);
                        document_to_json(document,
                                         obj,
                                         mode,
                                         max_paragraph_length,
                                         unique_values_only,
                                         text_as_tokens,
                                         tokens_length,
                                         token_padding,
                                         pooling_mode,
                                         overlap_ratio);
                        xmlFreeDoc(xml_doc);
                    }
                }
            }
                break;
            case document_type_xlsx:
            {
                std::vector<std::string> sst;
                opcPart sharedStrings = opcPartFind(container, _X("/xl/sharedStrings.xml"), NULL, 0);
                if(sharedStrings){
                    xmlDoc *xml_sharedStrings = parse_opc_part(container, sharedStrings);
                    if(xml_sharedStrings) {
                        xmlNode *table_root = xmlDocGetRootElement(xml_sharedStrings);
                        if(table_root) {
                            for (xmlNodePtr si = table_root ? table_root->children : nullptr; si; si = si->next) {
                                if(si->type == XML_ELEMENT_NODE){
                                    if (!xmlStrcmp(si->name, (const xmlChar *)"si")) {
                                        std::string s = xml_get_text_concat(si, "t");
                                        sst.emplace_back(std::move(s));
                                    }
                                }
                            }
                        }
                        xmlFreeDoc(xml_sharedStrings);
                    }
                }
                    
                xmlDoc *workbookDoc = parse_opc_part(container, part);
                if (workbookDoc) {
                    xmlNode *root = xmlDocGetRootElement(workbookDoc);
                    if (root) {
                        for (xmlNode *node = root->children; node; node = node->next) {
                            if (node->type == XML_ELEMENT_NODE && xmlStrcmp(node->name, BAD_CAST "sheets") == 0) {
                                for (xmlNode *sheetNode = node->children; sheetNode; sheetNode = sheetNode->next) {
                                    if (sheetNode->type == XML_ELEMENT_NODE && xmlStrcmp(sheetNode->name, BAD_CAST "sheet") == 0) {
                                        std::string sheetName;
                                        xmlChar *nameAttr = xmlGetProp(sheetNode, BAD_CAST "name");
                                        if (nameAttr) {
                                            sheetName = (reinterpret_cast<char *>(nameAttr));
                                            xmlFree(nameAttr);
                                        }
                                        xmlChar *idAttr = xmlGetProp(sheetNode, BAD_CAST "id");
                                        if (idAttr) {
                                            opcRelation target = opcRelationFind(container, part, idAttr, _X("http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet"));
                                            xmlFree(idAttr);
                                            opcPart sheet = opcRelationGetInternalTarget(container, part, target);
                                            if(sheet){
                                                xmlDoc *xml_sheet = parse_opc_part(container, sheet);
                                                if(xml_sheet) {
                                                    xmlNode *slide_root = xmlDocGetRootElement(xml_sheet);
                                                    if(slide_root) {
                                                        Sheet sheet;
                                                        sheet.name = sheetName;
                                                        workbook.sheets.push_back(sheet);
                                                        process_worksheet(slide_root, workbook, "row", sst);
                                                    }
                                                    xmlFreeDoc(xml_sheet);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                document_to_json_ss(workbook,
                                    obj,
                                    mode,
                                    max_paragraph_length,
                                    unique_values_only,
                                    text_as_tokens,
                                    tokens_length,
                                    token_padding,
                                    pooling_mode,
                                    overlap_ratio);
            }
                break;
            case document_type_pptx:
            {
                int i = 0;
                opcPart slide = NULL;
                do {
                    i++;
                    std::vector<char>buf(1024);
                    snprintf(buf.data(), buf.size(), "/ppt/slides/slide%d.xml", i);
                    std::string slideName(buf.data());
                    slide = opcPartFind(container, _X(slideName.c_str()), NULL, 0);
                    if(slide){
                        xmlDoc *xml_slide = parse_opc_part(container, slide);
                        if(xml_slide) {
                            xmlNode *slide_root = xmlDocGetRootElement(xml_slide);
                            if(slide_root) {
                                Page _page;
                                document.pages.push_back(_page);
                                process_document(slide_root, document, "p", type);
                            }
                            xmlFreeDoc(xml_slide);
                        }
                    }
                } while (slide);
                document_to_json(document,
                                 obj,
                                 mode,
                                 max_paragraph_length,
                                 unique_values_only,
                                 text_as_tokens,
                                 tokens_length,
                                 token_padding,
                                 pooling_mode,
                                 overlap_ratio);
            }
                break;
            default:
                break;
        }
    }
    
    opcContainerClose(container, OPC_CLOSE_NOW);

    goto finally;
    
unfortunately:

    ob_set_a(obj, L"type", L"unknown");
    return false;
        
finally:
    
    ob_set_b(obj, L"success", true);
    return true;
}
