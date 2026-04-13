//
//  xls-parser
//
//  Created by miyako on 2025/10/02 as CLI
//  Converted by miyako to function on 2026/04/13
//

#include "xls-parser.h"

namespace xls {
    struct Row {
        std::vector<std::string> cells;
    };

    struct Sheet {
        std::string name;
        std::vector<Row> rows;
    };

    struct Document {
        std::string type;
        std::vector<Sheet> sheets;
    };
}

using namespace xls;

static void ob_append_s(PA_CollectionRef c, const std::string& value) {
    
    C_TEXT t;
    t.setUTF8String((const uint8_t *)value.c_str(), (uint32_t)value.length());
    PA_Variable v = PA_CreateVariable(eVK_Unistring);
    PA_Unistring u = PA_CreateUnistring((PA_Unichar *)t.getUTF16StringPtr());
    PA_SetStringVariable(&v, &u);
    PA_SetCollectionElement(c, PA_GetCollectionLength(c), v);
    PA_ClearVariable(&v);
}

static void ob_append_c(PA_CollectionRef c, PA_CollectionRef value) {

    PA_Variable v = PA_CreateVariable(eVK_Collection);
    PA_SetCollectionVariable(&v, value);
    PA_SetCollectionElement(c, PA_GetCollectionLength(c), v);
    PA_ClearVariable(&v);
}

static void ob_append_o(PA_CollectionRef c, PA_ObjectRef value) {

    PA_Variable v = PA_CreateVariable(eVK_Object);
    PA_SetObjectVariable(&v, value);
    PA_SetCollectionElement(c, PA_GetCollectionLength(c), v);
    PA_ClearVariable(&v);
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

static std::string conv(const std::string& input, const std::string& charset) {
    
    std::string str = input;
    
    iconv_t cd = iconv_open("utf-8", charset.c_str());
    
    if (cd != (iconv_t)-1) {
        size_t inBytesLeft = input.size();
        size_t outBytesLeft = (inBytesLeft * 4)+1;
        std::vector<char> output(outBytesLeft);
        char *inBuf = const_cast<char*>(input.data());
        char *outBuf = output.data();
        if (iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft) != (size_t)-1) {
            str = std::string(output.data(), output.size() - outBytesLeft);
        }
        iconv_close(cd);
    }
    
    return str;
}

bool xls_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                    output_type mode,
                    int max_paragraph_length,
                    bool unique_values_only,
                    bool text_as_tokens,
                    int tokens_length,
                    bool token_padding,
                    int pooling_mode,
                    float overlap_ratio,
                    std::string charset) {
       
    Document document;
    
    xls_error_t errorCode;
    xlsWorkBook *pWB = xls_open_buffer((const unsigned char *)data.data(),
                                       (size_t)data.size(),
                                       (const char *)charset.c_str(), &errorCode);
    if(pWB) {
        
        document.type = "xls";
        
        errorCode = xls_parseWorkBook(pWB);
        if(errorCode) {
            std::cerr << "fail:xls_parseWorkBook(" << errorCode << ")" << xls_getError(errorCode) << std::endl;
        }else{
            for (uint16_t i = 0; i < pWB->sheets.count; ++i) {
                xlsWorkSheet* pWS = xls_getWorkSheet(pWB, i);
                if (!pWS) {
                    std::cerr << "fail:xls_getWorkSheet" << std::endl;
                    continue;
                }
                
                errorCode = xls_parseWorkSheet(pWS);
                
                if(errorCode) {
                    std::cerr << "fail:xls_parseWorkSheet(" << errorCode << ")" << xls_getError(errorCode) << std::endl;
                }else{
                    
                    Sheet sheet;
                    sheet.name = pWB->sheets.sheet[i].name;
                    document.sheets.push_back(sheet);
                    
                    for (xls::DWORD row = 0; row <= pWS->rows.lastrow; ++row) {
                        
                        Row _row;
                        
                        for (xls::DWORD col = 0; col <= pWS->rows.lastcol; ++col) {
                            xlsCell* cell = xls_cell(pWS, row, col);
                            if (cell && cell->str) {
                                std::string str = cell->str;
                                _row.cells.push_back(conv(str, charset));
                            }
                        }
                        sheet.rows.push_back(_row);
                    }
                    
                    document.sheets.push_back(sheet);
                }
                xls_close_WS(pWS);
            }
        }
        xls_close_WB(pWB);
    }else{
        std::cerr << "fail:xls_open_buffer(" << errorCode << ")" << xls_getError(errorCode) << std::endl;
    }
        
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

    goto finally;
    
unfortunately:

    ob_set_a(obj, L"type", L"unknown");
    return false;
        
finally:
    
    ob_set_b(obj, L"success", true);
    return true;
}
