//
//  main.cpp
//  txt-parser
//
//  Created by miyako on 2025/10/02.
//

#include "txt-parser.h"

namespace txt {
struct Document {
    std::string type;
    std::string text;
};
}

using namespace txt;

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
            if(!document.text.empty()) {
                ob_append_s(pages, document.text);
            }
            ob_set_c(documentNode, L"documents", pages);
        }
            break;
        case output_type_collection:
        {
            ob_set_s(documentNode, "type", document.type.c_str());
            std::vector<std::string> texts;
            if(!document.text.empty()) {
                texts.push_back(document.text);
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
            if(!document.text.empty()) {
                texts.push_back(document.text);
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
            if(!document.text.empty()) {
                PA_ObjectRef pageNode = PA_CreateObject();
                PA_CollectionRef paragraphs = PA_CreateCollection();
                PA_ObjectRef paragraphNode = PA_CreateObject();
                ob_set_n(paragraphNode, "index", 0);
                ob_set_s(paragraphNode, "text", document.text.c_str());
                ob_append_o(paragraphs, paragraphNode);
                
                ob_set_c(pageNode, "paragraphs", paragraphs);
                ob_set_n(pageNode, "index", 0);
                ob_append_o(pages, pageNode);
            }
            ob_set_c(documentNode, L"pages", pages);
        }
            break;
    }
}

extern bool txt_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                           output_type mode,
                           int max_paragraph_length,
                           bool unique_values_only,
                           bool text_as_tokens,
                           int tokens_length,
                           bool token_padding,
                           int pooling_mode,
                           float overlap_ratio) {
            
    Document document;
    document.type = "txt";
    document.text = std::string((const char *)data.data(), data.size());
    
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
    
    ob_set_b(obj, L"success", true);
    return true;
}
