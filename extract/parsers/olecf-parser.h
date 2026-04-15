#ifndef __OLECF_PARSER_H__
#define __OLECF_PARSER_H__

#ifndef FILE_MACROS_H
    #define FILE_MACROS_H
    #define BUFLEN 4096
    #ifdef __APPLE__
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

//experimental
#define WITH_NATIVE_RTF_CONVERT 1

#include <string>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "libolecf.h"

#define WORDDOCUMENT_STREAM "WordDocument"
#define TABLE_STREAM_0 "0Table"
#define TABLE_STREAM_1 "1Table"

#define FIB_BASE_SIZE 32
#define FIB_OFFSET_FWHICHTBLSTM 0x0A
#define FIB_OFFSET_FCCLX 0x01A2
#define FIB_OFFSET_LCBCLX 0x01A6

#if WITH_NATIVE_RTF_CONVERT
    #ifdef __APPLE__
        #include <Foundation/Foundation.h>
        #include <AppKit/AppKit.h>
    #else
        #include <windows.h>
        #include <richedit.h>
        #include <commctrl.h>
        #include <tchar.h>
    #endif
#else
    #ifdef __APPLE__
    #include <unistd.h>
    #include <CoreFoundation/CoreFoundation.h>
    #include "librtf.h"
    #else
    #include "librtf (windows).h"
    #endif
    #include "RtfReader.h"
#endif

#include <tidy.h>
#include <tidybuffio.h>

#ifdef _WIN32
//#define _unlink DeleteFile
#define _libolecf_file_open libolecf_file_open_wide
#else
#define HWND char*
#define _unlink unlink
#define _libolecf_file_open libolecf_file_open
#endif

#include <json/json.h>
#include "4DPluginAPI.h"
#include "C_TEXT.h"
#include "4DPlugin-JSON.h"
#include "tokenizers_cpp.h"
#include "4DPlugin-Universal-Document-Parser.h"

#ifdef _WIN32
    extern HMODULE ghmodule;
#endif

extern bool olecf_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                             output_type mode,
                             int max_paragraph_length,
                             bool unique_values_only,
                             bool text_as_tokens,
                             int tokens_length,
                             bool token_padding,
                             int pooling_mode,
                             float overlap_ratio,
                             int codepage);

#endif  /* __OLECF_PARSER_H__ */
