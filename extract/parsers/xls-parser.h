#ifndef __XLS_PARSER_H__
#define __XLS_PARSER_H__

#ifndef FILE_MACROS_H
    #define FILE_MACROS_H
    #define BUFLEN 4096
    #if VERSIONMAC
        #define _fopen fopen
        #define _fseek fseek
        #define _ftell ftell
        #define _rb "rb"
        #define _wb "wb"
    #else
        #define _fopen _wfopen
        #define _fseek _fseeki64
        #define _ftell _ftelli64
        #define _rb L"rb"
        #define _wb L"wb"
    #endif
#endif

#ifndef OUTPUT_TYPES
    #define OUTPUT_TYPES
    typedef enum {
        output_type_object = 0,
        output_type_text,
        output_type_collection,
        output_type_collections
    }output_type;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <locale>
#include <fstream>
#include <sstream>
#include <iostream>

#include "xls.h"
#include <iconv.h>

#ifdef _WIN32
    #define NOMINMAX
    #define _SSIZE_T_DEFINED 1
    #include <Windows.h>
#endif

#include <json/json.h>
#include "4DPluginAPI.h"
#include "C_TEXT.h"
#include "4DPlugin-JSON.h"
#include "tokenizers_cpp.h"
#include "4DPlugin-Universal-Document-Parser.h"

extern bool xls_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                           output_type mode,
                           int max_paragraph_length,
                           bool unique_values_only,
                           bool text_as_tokens,
                           int tokens_length,
                           bool token_padding,
                           int pooling_mode,
                           float overlap_ratio,
                           std::string charset);

#endif  /* __XLS_PARSER_H__ */
