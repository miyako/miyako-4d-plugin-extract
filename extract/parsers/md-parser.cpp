//
//  md-parser
//
//  Created by miyako on 2026/04/13.
//

#include "md-parser.h"

namespace md {
    struct Section {
        std::string heading;   // e.g. "## Authentication"
        std::string body;      // everything under that heading until the next one
    };

    struct Page {
        std::vector<Section> sections;
    };

    struct Document {
        std::string type;
        std::vector<Page> pages;
    };
}

using namespace md;

static void document_to_json(Document& document,
                             PA_ObjectRef documentNode,
                             output_type mode,
                             int max_paragraph_length,
                             bool unique_values_only,
                             bool text_as_tokens,
                             int tokens_length,
                             bool token_padding,
                             int pooling_mode,
                             float overlap_ratio,
                             bool break_by_section) {

    switch (mode) {
        case output_type_text:
        {
            PA_CollectionRef pages = PA_CreateCollection();
            for (const auto &page : document.pages) {
                bool emptyCol = true;
                std::string paragraphs;
                std::unordered_set<std::string> seen;
                int paragraph_length = 0;
                for (const auto &section : page.sections) {
                    if(section.body.empty())
                        continue;
                                        
                    if ((unique_values_only) && (!seen.insert(section.body).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    if (break_by_section) {
                        paragraphs = section.body;
                        ob_append_s(pages, paragraphs);
                        emptyCol = true;
                    }else {
                        if(!paragraphs.empty()) paragraphs += "\n";
                        paragraphs += section.body;
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
                }
                if (emptyCol) {
                    //empty page
                    continue;
                }
                ob_append_s(pages, paragraphs);
            }
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
                for (const auto &section : page.sections) {
                    if(section.body.empty())
                        continue;
                    
                    if ((unique_values_only) && (!seen.insert(section.body).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    if (break_by_section) {
                        paragraphs = section.body;
                        texts.push_back(paragraphs);
                        emptyCol = true;
                    }else {
                        if(!paragraphs.empty()) paragraphs += "\n";
                        paragraphs += section.body;
                        
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
                for (const auto &section : page.sections) {
                    if(section.body.empty())
                        continue;
                    
                    if(section.body.empty())
                        continue;
                    
                    if ((unique_values_only) && (!seen.insert(section.body).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    texts.push_back(section.body);
                    
                    if (break_by_section) {
                        PA_CollectionRef matrix = process_paragraphs(texts, tokens_length, token_padding, !text_as_tokens, overlap_ratio, pooling_mode);
                        ob_append_c(pages, matrix);
                        emptyCol = true;
                        texts.clear();
                    } else {
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
                for (const auto &section : page.sections) {
                    PA_ObjectRef paragraphNode = PA_CreateObject();
                    if(section.body.empty())
                        continue;
                    
                    emptyCol = false;
                                        
                    ob_set_n(paragraphNode, "index", rowIdx++);
                    ob_set_s(paragraphNode, "text", section.body.c_str());
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

static int heading_level(const std::string &line) {
    int lvl = 0;
    while (lvl < (int)line.size() && line[lvl] == '#') ++lvl;
    if (lvl == 0 || lvl > 3) return 0;
    if (line.size() > (size_t)lvl && line[lvl] == ' ') return lvl;
    return 0;
}

static std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::vector<std::string> split_lines(const std::string &s) {
    std::vector<std::string> lines;
    std::istringstream ss(s);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
    return lines;
}

static std::vector<Section> parse_sections(const std::string &markdown) {
    std::vector<Section> sections;
    auto lines = split_lines(markdown);
 
    Section current;
    current.heading = "";  // top-level content before first heading
 
    for (auto &line : lines) {
        int lvl = heading_level(line);
        if (lvl > 0) {
            // Flush current section if it has content
            if (!trim(current.body).empty() || !current.heading.empty())
                sections.push_back(current);
            current.heading = line;
            current.body.clear();
        } else {
            current.body += line + "\n";
        }
    }
    if (!trim(current.body).empty() || !current.heading.empty())
        sections.push_back(current);
 
    return sections;
}

static std::vector<std::string> split_sentences(const std::string &text) {
    std::vector<std::string> sents;
    size_t start = 0;
    for (size_t i = 0; i + 1 < text.size(); ++i) {
        if ((text[i] == '.' || text[i] == '!' || text[i] == '?') &&
            text[i + 1] == ' ') {
            sents.push_back(trim(text.substr(start, i - start + 1)));
            start = i + 2;
        }
    }
    if (start < text.size())
        sents.push_back(trim(text.substr(start)));
    return sents;
}

extern bool md_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                          output_type mode,
                          int max_paragraph_length,
                          bool unique_values_only,
                          bool text_as_tokens,
                          int tokens_length,
                          bool token_padding,
                          int pooling_mode,
                          float overlap_ratio,
                          bool break_by_section) {
            
    Document document;
    document.type = "md";
    std::string markdown = std::string((const char *)data.data(), data.size());
    
    auto sections = parse_sections(markdown);
    
    Page page;
    for (auto &section : sections) {
        page.sections.push_back(section);
    }
    document.pages.push_back(page);
    
    document_to_json(document,
                     obj,
                     mode,
                     max_paragraph_length,
                     unique_values_only,
                     text_as_tokens,
                     tokens_length,
                     token_padding,
                     pooling_mode,
                     overlap_ratio,
                     break_by_section);
        
    ob_set_b(obj, L"success", true);
    return true;
}
