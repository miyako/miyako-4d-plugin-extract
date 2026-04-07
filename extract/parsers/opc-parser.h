#ifndef __OPC_PARSER_H__
#define __OPC_PARSER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <array>
#include <locale>
#include <fstream>

#ifdef _WIN32
    #define NOMINMAX
    #define _SSIZE_T_DEFINED 1
    #include <Windows.h>
#endif

#include <cybozu/mmap.hpp>
#include <cybozu/file.hpp>
#include <cybozu/atoi.hpp>
#include <cybozu/option.hpp>
#include "cfb.hpp"
#include "decode.hpp"
#include "encode.hpp"
#include "make_dataspace.hpp"
#ifdef _MSC_VER
    #include <cybozu/string.hpp>
#endif

#include <opc/opc.h>
#include <json/json.h>
#include <sstream>
#include <iostream>
#include <unordered_set>

#include "4DPluginAPI.h"

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

typedef enum {
    
    document_type_docx = 0,
    document_type_xlsx,
    document_type_pptx,
    document_type_unknown = -1
    
}document_type;

typedef enum {
    output_type_object = 0,
    output_type_text,
    output_type_collection,
    output_type_collections
}output_type;

#include "C_TEXT.h"
#include "4DPlugin-JSON.h"
#include "tokenizers_cpp.h"
#include "4DPlugin-Universal-Document-Parser.h"

extern bool opc_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                           output_type mode,
                           int max_paragraph_length,
                           bool unique_values_only,
                           bool text_as_tokens,
                           int tokens_length,
                           bool token_padding,
                           int pooling_mode,
                           float overlap_ratio,
                           std::string password);

#endif  /* __OPC_PARSER_H__ */
