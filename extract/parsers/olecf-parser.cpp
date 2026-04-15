//
//  libolecf-parser
//
//

#include "olecf-parser.h"

#ifdef __APPLE__
static CFStringEncoding codepage_to_cfencoding(int cp) {
    
    switch(cp) {
   
        case 1361: return kCFStringEncodingWindowsKoreanJohab;
        case 1258: return kCFStringEncodingWindowsVietnamese;
        case 1257: return kCFStringEncodingWindowsBalticRim;
        case 1256: return kCFStringEncodingWindowsArabic;
        case 1255: return kCFStringEncodingWindowsHebrew;
        case 1254: return kCFStringEncodingWindowsLatin5;
        case 1253: return kCFStringEncodingWindowsGreek;
        case 1252: return kCFStringEncodingWindowsLatin1;
        case 1251: return kCFStringEncodingWindowsCyrillic;
        case 1250: return kCFStringEncodingWindowsLatin2;
        case 950 : return kCFStringEncodingBig5;
        case 949 : return kCFStringEncodingDOSKorean;
        case 936 : return kCFStringEncodingDOSChineseSimplif;
        case 932 : return kCFStringEncodingShiftJIS;
        case 869 : return kCFStringEncodingDOSGreek2;
        case 866 : return kCFStringEncodingDOSRussian;
        case 865 : return kCFStringEncodingDOSNordic;
        case 864 : return kCFStringEncodingDOSArabic;
        case 863 : return kCFStringEncodingDOSCanadianFrench;
        case 862 : return kCFStringEncodingDOSHebrew;
        case 861 : return kCFStringEncodingDOSIcelandic;
        case 860 : return kCFStringEncodingDOSPortuguese;
        case 857 : return kCFStringEncodingDOSTurkish;
        case 855 : return kCFStringEncodingDOSCyrillic;
        case 852 : return kCFStringEncodingDOSLatin2;
        case 851 : return kCFStringEncodingDOSGreek1;
        case 850 : return kCFStringEncodingDOSLatin1;
        case 775 : return kCFStringEncodingDOSBalticRim;
        case 737 : return kCFStringEncodingDOSGreek;
        case 437 : return kCFStringEncodingDOSLatinUS;
        case 37  : return kCFStringEncodingEBCDIC_CP037;
        default:   return 1252; // default
    }
}
#endif

static std::string filter_control_characters(const std::string& input) {
    
    std::string output;
    
    bool inField = false;
    
    for (size_t i = 0; i < input.length(); ++i) {
        unsigned char ch = input[i];
        
        // Skip Word field codes: 0x13, 0x14, 0x15
        if (ch == 0x13) {  // Field start
            inField = true;
            continue;
        }
        if (ch == 0x14) {  // Field separator
            continue;
        }
        if (ch == 0x15) {  // Field end
            inField = false;
            continue;
        }
        
        // Optionally skip text inside fields
        if (inField) continue;
        
        // Convert carriage return (0x0D) to newline
        if (ch == '\r') {
            output += '\n';
            continue;
        }

        // Skip control characters under 0x20 except newline/tab
        if (ch < 0x20 && ch != '\n' && ch != '\t') {
            continue;
        }
        
        output += ch;
    }
    
    return output;
    
}

static void utf16_to_utf8(const uint8_t *u16data, size_t u16size, std::string& u8) {
    
#ifdef __APPLE__
    CFStringRef str = CFStringCreateWithCharacters(kCFAllocatorDefault, (const UniChar *)u16data, u16size);
    if(str){
        size_t size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8) + sizeof(uint8_t);
        std::vector<uint8_t> buf(size+1);
        CFIndex len = 0;
        CFStringGetBytes(str, CFRangeMake(0, CFStringGetLength(str)), kCFStringEncodingUTF8, 0, true, (UInt8 *)buf.data(), buf.size(), &len);
        u8 = (const char *)buf.data();
        CFRelease(str);
    }else{
        u8 = "";
    }
#else
    int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)u16data, u16size, NULL, 0, NULL, NULL);
    if(len){
        std::vector<uint8_t> buf(len + 1);
        WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)u16data, u16size, (LPSTR)buf.data(), buf.size(), NULL, NULL);
        u8 = (const char *)buf.data();
    }else{
        u8 = "";
    }
#endif
}

static void ansi_to_utf8(std::string& ansi, std::string& u8, int cp) {
    
#ifdef __APPLE__
    CFStringEncoding encoding = codepage_to_cfencoding(cp);
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault,
                                              reinterpret_cast<const UInt8*>(ansi.data()),
                                              ansi.size(),
                                              encoding,
                                              false);
    if(str) {
        CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str),
                                                        kCFStringEncodingUTF8);
        std::vector<uint8_t> buf(len + 1);
        if (CFStringGetCString(str, (char *)buf.data(), buf.size(), kCFStringEncodingUTF8)) {
            u8 = (const char *)buf.data();
        }else{
            u8 = "";
        }
        CFRelease(str);
    }else{
        u8 = "";
    }
#else
    int len = MultiByteToWideChar(cp, 0, (LPCSTR)ansi.data(), ansi.size(), NULL, 0);
    if(len){
        std::vector<uint16_t> buf(len + sizeof(uint16_t));
        MultiByteToWideChar(cp, 0, (LPCSTR)ansi.data(), ansi.size(), (LPWSTR)buf.data(), buf.size());
        utf16_to_utf8((const uint8_t *)buf.data(), buf.size(), u8);
    }else{
        u8 = "";
    }
#endif
}

namespace olecf {
    struct Account {
        std::string name;
        std::string address;
    };

    struct Message {
        std::string subject;
        std::string text;
        std::string html;
        std::string rtf;
        Account sender;
        Account recipient;
        std::string headers;
    };

    struct Slide {
        std::vector<std::string> text;
    };

    struct Document {
        std::string type;
        Message message;
        uint32_t fcClx;
        uint32_t lcbClx;
        std::string docVersion;
        std::string text;
        uint16_t docLanguageId;
        uint8_t fWhichTblStm;
        std::vector<Slide> slides;
    };
}

static void utf16le_to_utf_8(std::vector<uint8_t>& buf, std::string& u8) {
    
    buf.insert(buf.end(), 2, 0x00);
    
#ifdef __APPLE__
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault,
                                              (const UInt8 *)buf.data(),
                                              buf.size()*sizeof(UniChar), kCFStringEncodingUTF16LE, true);
    if(str)
    {
        CFIndex len = CFStringGetLength(str);
        CFIndex maxSize = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
        std::vector<uint8_t> _buf(maxSize);
        if (CFStringGetCString(str, (char *)buf.data(), maxSize, kCFStringEncodingUTF8)) {
            u8 = (const char *)buf.data();
        }
        CFRelease(str);
    }
#else
    int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)buf.data(), buf.size(), NULL, 0, NULL, NULL);
    std::vector<uint8_t> bufu8(len + 1);
    if (WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)buf.data(), buf.size(), (LPSTR)bufu8.data(), len, NULL, NULL)) {
        u8 = (const char*)bufu8.data();
    }
#endif
}

#if defined(_WIN32)
static int create_temp_file_path(std::wstring& path) {
    std::vector<wchar_t> dirBuf(MAX_PATH);
    DWORD dirLen = GetTempPathW((DWORD)dirBuf.size(), dirBuf.data());
    if (dirLen == 0 || dirLen > dirBuf.size()) return -1;

    std::vector<wchar_t> fileBuf(MAX_PATH);
    if (GetTempFileNameW(dirBuf.data(), L"pff", 0, fileBuf.data()) == 0)
        return -1;

    path.assign(fileBuf.data());
    return 0;
}
#else
static int create_temp_file_path(std::string& path) {
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    std::vector<char>buf(1024);
    snprintf(buf.data(), buf.size(), "%s/pffXXXXXX", tmpdir);
    path = std::string(buf.data());
    int fd = mkstemp((char *)path.c_str());
    if (fd == -1) return -1;
    close(fd);
    return 0;
}
#endif

static int process_rtf(std::vector<uint8_t>& buf, std::vector<uint8_t>& rtf) {
    
    /*
     3.1.1.2 Compressed RTF Header
     https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-oxrtfcp/742dec57-50e8-460b-9aaf-504816a7c3de
     */
    
    uint32_t pos = 0x0;
    
    if(buf.size() < 0x10) {
        return -1;//no header!
    }
        
    uint32_t COMPSIZE =
        (uint32_t)buf.at(0)
        | ((uint32_t)buf.at(1) << 8)
        | ((uint32_t)buf.at(2) << 16)
        | ((uint32_t)buf.at(3) << 24);

    if(COMPSIZE != (buf.size() - 0x4)) {
        return -1;//corrupt file!
    }
    
    pos += 4;

    uint32_t RAWSIZE =
        (uint32_t)buf.at(4)
        | ((uint32_t)buf.at(5) << 8)
        | ((uint32_t)buf.at(6) << 16)
        | ((uint32_t)buf.at(7) << 24);

    pos += 4;

    uint32_t COMPRESSED =
        (uint32_t)buf.at(8)
        | ((uint32_t)buf.at(9) << 8)
        | ((uint32_t)buf.at(10) << 16)
        | ((uint32_t)buf.at(11) << 24);

    if(COMPRESSED == 0x414c454d) {
        rtf = buf;
        return 0;//not compressed
    }
    
    pos+=4;
        
    if(COMPRESSED == 0x75465a4c) {
        
        std::string DICT = "{\\rtf1\\ansi\\mac\\deff0\\deftab720{\\fonttbl;}{\\f0\\fnil \\froman \\fswiss \\fmodern \\fscript \\fdecor MS Sans SerifSymbolArialTimes New RomanCourier{\\colortbl\\red0\\green0\\blue0\r\n\\par \\pard\\plain\\f0\\fs20\\b\\i\\u\\tab\\tx";
        
        //CRC
        
        pos += 4;
        
        size_t ReadOffset  = 0;
        size_t WriteOffset = DICT.length();//207

        rtf.clear();
                
        size_t end = buf.size();
        
        while(pos < end) {
            
            uint8_t CONTROL = buf.at(pos);
            pos++;
            
            bool bits[8];
            bits[0] = (CONTROL & 0b00000001);
            bits[1] = (CONTROL & 0b00000010);
            bits[2] = (CONTROL & 0b00000100);
            bits[3] = (CONTROL & 0b00001000);
            bits[4] = (CONTROL & 0b00010000);
            bits[5] = (CONTROL & 0b00100000);
            bits[6] = (CONTROL & 0b01000000);
            bits[7] = (CONTROL & 0b10000000);
            uint8_t count0 = 0;
            uint8_t count1 = 0;
            for(size_t i = 0; i < sizeof(bits); i++) {
                if(bits[i]) {
                    count1++;
                }else{
                    count0++;
                }
            }

            for(uint8_t b = 0; b < sizeof(bits); b++) {

                if(bits[b]) {
                    //If the value of the bit is 1:
                    //Read a 16-bit dictionary reference from the input in big-endian byte-order.
                    uint8_t u = buf.at(pos);
                    uint8_t l = buf.at(pos+1);
                    pos+=2;
                    
                    //Extract the offset from the dictionary reference
                    uint32_t dictref = (u << 8) + l;
                    ReadOffset = (dictref & 0b1111111111110000) >>4 ;
                    
                    //Compare the offset to the dictionary's write offset
                    //if they are equal, then the decompression is complete; exit the decompression loop
                    if(WriteOffset == ReadOffset) {
                        //escape hatch
                        goto end_of_decompression;
                    }
                    //If they are not equal, continue to the next step.
                    //Set the dictionary's read offset to offset
                    
                    //Extract the length from the dictionary reference
                    //and calculate the actual length by adding 2 to the
                    //length that is extracted from the dictionary reference.
                    uint32_t len = (dictref & 0b0000000000001111) + 2;
                    
                    uint32_t i = 0;
                    while (i != len) {
                        
                        //Read a byte from the current dictionary read offset...
                        uint8_t v = DICT.at(ReadOffset);
                        i++;
                        //...and write it to the output.
                        rtf.insert(rtf.end(), v);
//                        std::cerr << v;
                        
                        //Increment the read offset
                        ReadOffset++;
                        if(ReadOffset == 4096) {
                            ReadOffset=0;//circular
                        }
                        
                        //Write the byte to the dictionary at the write offset.
                        if(WriteOffset == DICT.length()) {
                            DICT.push_back(v);
                        }else{
                            DICT[WriteOffset] = v;
                        }
                        
                        //Increment the write offset
                        WriteOffset++;
                        if(WriteOffset == 4096) {
                            WriteOffset=0;//circular
                        }
                        
                        // Continue from step 6 until the number of bytes calculated in step 5 has been read from the dictionary
                    }

                }else{
                           
                    //If the value of the bit is 0:
                    uint8_t v = buf.at(pos);
                    //Read a 1-byte literal from the input...
                    pos++;
                    //...and write it to the output.
                    rtf.insert(rtf.end(), v);
//                    std::cerr << v;
                    
                    //Set the byte in the dictionary at the current write offset.
                    if(WriteOffset == DICT.length()) {
                        DICT.push_back(v);
                    }else{
                        DICT[WriteOffset] = v;
                    }
                    
                    //Increment the write offset
                    WriteOffset++;
                    if(WriteOffset == 4096) {
                        WriteOffset=0;//circular
                    }

                }
 
                if(rtf.size() == RAWSIZE) {
                    //escape hatch
                    goto end_of_decompression;
                }
            }
        }
        
        end_of_decompression:
              
        return 0;
    }
    return -1;//not rtf
}

static void print_text(TidyDoc tdoc, TidyNode tnode, std::string& text) {
    
    for (TidyNode child = tidyGetChild(tnode); child; child = tidyGetNext(child)) {
        TidyNodeType ttype = tidyNodeGetType(child);
        if (ttype == TidyNode_Text) {
            TidyBuffer buf;
            tidyBufInit(&buf);
            tidyNodeGetValue(tdoc, child, &buf);
            text += std::string((char*)buf.bp, buf.size);
            tidyBufFree(&buf);
        } else if (ttype == TidyNode_Start) {
            print_text(tdoc, child, text);
        }
    }
}

static void html_to_txt_tidy(std::string& html, std::string& txt) {
    
    TidyDoc tdoc = tidyCreate();
    TidyBuffer errbuf;
    tidyBufInit(&errbuf);
    
    tidyOptSetBool(tdoc, TidyXhtmlOut, yes);
    tidyOptSetBool(tdoc, TidyXmlOut, no);
    tidyOptSetBool(tdoc, TidyForceOutput, yes);
    
    tidyOptSetBool(tdoc, TidyQuiet, yes);
    tidyOptSetBool(tdoc, TidyShowWarnings, no);
    tidySetErrorBuffer(tdoc, &errbuf);

    tidyOptSetValue(tdoc, TidyCustomTags, "blocklevel");
    tidyOptSetValue(tdoc, TidyDoctype, "auto");
    
    tidyOptSetBool(tdoc, TidyMark, no);
    tidyOptSetInt(tdoc, TidyWrapLen, 0);
    tidyOptSetBool(tdoc, TidyDropEmptyElems, yes);
    tidyOptSetBool(tdoc, TidyDropEmptyParas, yes);
    tidyOptSetBool(tdoc, TidyDropPropAttrs, yes);

    tidyOptSetBool(tdoc, TidyIndentContent, no);
    tidyOptSetInt(tdoc, TidyIndentSpaces, 0);

    tidyOptSetBool(tdoc, TidyQuoteAmpersand, no);
    tidyOptSetBool(tdoc, TidyAsciiChars, no);
    tidyOptSetBool(tdoc, TidyPreserveEntities, no);
    tidyOptSetBool(tdoc, TidyNumEntities, yes);
    
    if(tidyParseString(tdoc, html.c_str()) >= 0) {
        if(tidyCleanAndRepair(tdoc) >= 0) {
            TidyNode body = tidyGetBody(tdoc);
            print_text(tdoc, body, txt);
        }
    }
    
    tidyRelease(tdoc);
    tidyBufFree(&errbuf);
}

#if WITH_NATIVE_RTF_CONVERT
static void rtf_to_text_platform(HWND hwnd, std::string& rtf, std::string& text) {
#ifdef _WIN32
    SETTEXTEX st = {};
    st.flags = ST_DEFAULT;
    st.codepage = CP_ACP;
    SendMessage(hwnd, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)rtf.c_str());
    
    int len = GetWindowTextLength(hwnd);
    
    std::vector<uint16_t> buf((len + 1));
    GetWindowText(hwnd,(LPWSTR)buf.data(), buf.size());
    
    utf16_to_utf8((const uint8_t *)buf.data(), buf.size(), text);
#else
    
    NSData *src = [[NSData alloc]initWithBytes:rtf.c_str() length:rtf.length()];
    if(src) {
        NSError *error = nil;
        NSAttributedString *attrStr = [[NSAttributedString alloc] initWithData:src
                                                                       options:@{NSDocumentTypeDocumentOption: NSRTFTextDocumentType}
                                                            documentAttributes:nil
                                                                         error:&error];
        if (!error) {
            NSString *u8 = [attrStr string];
            text = (const char *)[u8 UTF8String];
        }
    }
#endif
}
#endif

using namespace olecf;

static void document_to_json_msg(Document& document,
                                 PA_ObjectRef documentNode,
                                 output_type mode,
                                 int max_paragraph_length,
                                 bool unique_values_only,
                                 bool text_as_tokens,
                                 int tokens_length,
                                 bool token_padding,
                                 int pooling_mode,
                                 float overlap_ratio,
                                 HWND hwnd) {
    
    if((document.message.html.length() != 0)
       && (document.message.text.length() == 0)) {
            html_to_txt_tidy(document.message.html, document.message.text);
    }
    
    if((document.message.rtf.length() != 0) && (document.message.text.length() == 0)) {
            rtf_to_text_platform(hwnd, document.message.rtf, document.message.text);
    }
    
    switch (mode) {
        case output_type_text:
        {
            std::string text;
            text = document.message.subject;
            if ((!text.empty()) && (!document.message.text.empty())) {
                text += "\n";
            }
            text += document.message.text;
            PA_CollectionRef pages = PA_CreateCollection();
            if(!text.empty()) {
                ob_append_s(pages, text);
            }
            ob_set_c(documentNode, L"documents", pages);
        }
            break;
        case output_type_collection:
        {
            std::string text;
            text = document.message.subject;
            if ((!text.empty()) && (!document.message.text.empty())) {
                text += "\n";
            }
            text += document.message.text;
            
            ob_set_s(documentNode, "type", document.type.c_str());
            std::vector<std::string> texts;
            if(!text.empty()) {
                texts.push_back(text);
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
            std::string text;
            text = document.message.subject;
            if ((!text.empty()) && (!document.message.text.empty())) {
                text += "\n";
            }
            text += document.message.text;
            
            if(!text.empty()) {
                texts.push_back(text);
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
            
            PA_ObjectRef metaNode = PA_CreateObject();
            ob_set_s(metaNode, "subject", document.message.subject.c_str());
            ob_set_s(metaNode, "headers", document.message.headers.c_str());
            PA_ObjectRef recipientNode = PA_CreateObject();
            ob_set_s(recipientNode, L"name", document.message.recipient.name.c_str());
            ob_set_s(recipientNode, L"address", document.message.recipient.address.c_str());
            ob_set_o(metaNode, "recipient", recipientNode);
            PA_ObjectRef senderNode = PA_CreateObject();
            ob_set_s(senderNode, L"name", document.message.sender.name.c_str());
            ob_set_s(senderNode, L"address", document.message.sender.address.c_str());
            ob_set_o(metaNode, "sender", senderNode);
            ob_set_o(documentNode, L"meta", metaNode);
            
            if(!document.text.empty()) {
                PA_ObjectRef pageNode = PA_CreateObject();
                PA_CollectionRef paragraphs = PA_CreateCollection();
                PA_ObjectRef paragraphNode = PA_CreateObject();
                ob_set_n(paragraphNode, "index", 0);
                ob_set_s(paragraphNode, "text", document.message.text.c_str());
                ob_set_s(paragraphNode, "html", document.message.html.c_str());
                ob_set_s(paragraphNode, "rtf", document.message.rtf.c_str());
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

static void document_to_json_doc(Document& document,
                                 PA_ObjectRef documentNode,
                                 output_type mode,
                                 int max_paragraph_length,
                                 bool unique_values_only,
                                 bool text_as_tokens,
                                 int tokens_length,
                                 bool token_padding,
                                 int pooling_mode,
                                 float overlap_ratio,
                                 HWND hwnd) {
    
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

static void document_to_json_ppt(Document& document,
                                 PA_ObjectRef documentNode,
                                 output_type mode,
                                 int max_paragraph_length,
                                 bool unique_values_only,
                                 bool text_as_tokens,
                                 int tokens_length,
                                 bool token_padding,
                                 int pooling_mode,
                                 float overlap_ratio,
                                 HWND hwnd){
    
    switch (mode) {
        case output_type_text:
        {
            PA_CollectionRef pages = PA_CreateCollection();
            for (const auto &slide : document.slides) {
                bool emptyCol = true;
                std::string paragraphs;
                std::unordered_set<std::string> seen;
                int paragraph_length = 0;
                for (const auto &t : slide.text) {
                    if(t.empty())
                        continue;
                                        
                    if ((unique_values_only) && (!seen.insert(t).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    if(!paragraphs.empty()) paragraphs += "\n";
                    paragraphs += t;
                    
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
            ob_set_c(documentNode, L"documents", pages);
        }
            break;
        case output_type_collection:
        {
            ob_set_s(documentNode, "type", document.type.c_str());
            std::vector<std::string> texts;
            for (const auto &slide : document.slides) {
                bool emptyCol = true;
                std::string paragraphs;
                //no need for colIdx, rowIdx
                std::unordered_set<std::string> seen;
                int paragraph_length = 0;
                for (const auto &t : slide.text) {
                    if(t.empty())
                        continue;
       
                    
                    if ((unique_values_only) && (!seen.insert(t).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    if(!paragraphs.empty()) paragraphs += "\n";
                    paragraphs += t;
                    
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
            for (const auto &slide : document.slides) {
                bool emptyCol = true;
                std::unordered_set<std::string> seen;
                int paragraph_length = 0;
                for (const auto &t : slide.text) {
                    if(t.empty())
                        continue;
                    
                    if ((unique_values_only) && (!seen.insert(t).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    texts.push_back(t);
                    
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
            for (const auto &slide : document.slides) {
                bool emptyCol = true;
                PA_ObjectRef pageNode = PA_CreateObject();
                PA_CollectionRef paragraphs = PA_CreateCollection();
                int rowIdx = 0; // physical sheet row index, increments regardless of empty rows
                for (const auto &t : slide.text) {
                    PA_ObjectRef paragraphNode = PA_CreateObject();
                    if(t.empty())
                        continue;
                    
                    emptyCol = false;
                                        
                    ob_set_n(paragraphNode, "index", rowIdx++);
                    ob_set_s(paragraphNode, "text", t.c_str());
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

typedef struct {
    uint16_t recVerInstance; //recVer(4)+recVerInstance(12)
    uint16_t recType;
    uint32_t recLen;
} RecordHeader;

typedef struct {
    uint32_t x;
    uint32_t y;
} PointStruct;

typedef struct {
    uint32_t numer;
    uint32_t denom;
} RatioStruct;

typedef struct {
    //8+8+8+8=32
    RecordHeader rh;
    PointStruct slideSize;
    PointStruct notesSize;
    RatioStruct serverZoom;
    
    //4+4=8
    uint32_t notesMasterPersistIdRef;
    uint32_t handoutMasterPersistIdRef;
    
    //2+2=4
    uint16_t firstSlideNumber;
    uint16_t slideSizeType;
    
    //1+1+1+1=4
    uint8_t fSaveWithFonts;
    uint8_t fOmitTitlePlace;
    uint8_t fRightToLeft;
    uint8_t fShowComments;
} DocumentAtom;

static uint16_t read_u16_le(const uint8_t *buf) { return buf[0] | (buf[1]<<8); }
static uint32_t read_u32_le(const uint8_t *buf) { return buf[0] | (buf[1]<<8) | (buf[2]<<16) | (buf[3]<<24); }
static uint32_t read_u32_be(const uint8_t *buf) { return buf[0] << 24 | (buf[1]<<16) | (buf[2]<<8) | (buf[3]); }

static RecordHeader read_RecordHeader(const uint8_t *p) {
    
    size_t offset = 0;
    
    RecordHeader rh;
    rh.recVerInstance = read_u16_le(p);
    offset += sizeof(uint16_t);
    rh.recType        = read_u16_le(p + offset);
    offset += sizeof(uint16_t);
    rh.recLen         = read_u32_le(p + offset);
    
    return rh;
}

static PointStruct read_PointStruct(const uint8_t *p) {
    
    size_t offset = 0;
    
    PointStruct ps;
    ps.x = read_u32_le(p);
    offset += sizeof(uint32_t);
    ps.y = read_u32_le(p + offset);
    
    return ps;
}

static RatioStruct read_RatioStruct(const uint8_t *p) {
    
    size_t offset = 0;
    
    RatioStruct rs;
    rs.numer = read_u32_le(p);
    offset += sizeof(uint32_t);
    rs.denom = read_u32_le(p + offset);
    
    return rs;
}

static DocumentAtom read_DocumentAtom(const uint8_t *p) {
    
    size_t offset = 0;
    
    DocumentAtom da;
    da.rh = read_RecordHeader(p);
    offset += sizeof(RecordHeader);
    
    da.slideSize      = read_PointStruct(p + offset);
    offset += sizeof(PointStruct);
    da.notesSize      = read_PointStruct(p + offset);
    offset += sizeof(PointStruct);
    da.serverZoom     = read_RatioStruct(p + offset);
    offset += sizeof(RatioStruct);

    da.notesMasterPersistIdRef     = read_u32_le(p + offset);
    offset += sizeof(uint32_t);
    da.handoutMasterPersistIdRef   = read_u32_le(p + offset);
    offset += sizeof(uint32_t);
 
    da.firstSlideNumber     = read_u16_le(p + offset);
    offset += sizeof(uint16_t);
    da.slideSizeType     = read_u16_le(p + offset);
    offset += sizeof(uint16_t);
    
    da.fSaveWithFonts = p[offset];
    offset += sizeof(uint8_t);
    da.fOmitTitlePlace = p[offset];
    offset += sizeof(uint8_t);
    da.fRightToLeft = p[offset];
    offset += sizeof(uint8_t);
    da.fShowComments = p[offset];
    
    return da;
}

void read_doc(Document& document, const uint8_t *stream, size_t stream_len, int codepage) {
   
    uint16_t wIdent = read_u16_le(stream);
    if(wIdent == 0xA5EC) {
        document.type = "doc";
        uint16_t nFib = read_u16_le(stream + 0x02);
        document.docLanguageId = read_u16_le(stream + 0x06);
//        uint16_t pnNext = read_u16_le(stream + 0x08);
        uint16_t ABCDEFGHIJKLM = read_u16_le(stream + 0x0A);
//        bool fDot       = ABCDEFGHIJKLM & 0x0001;
//        bool fGlsy      = ABCDEFGHIJKLM & 0x0002;
//        bool fComplex   = ABCDEFGHIJKLM & 0x0004;
//        bool fHasPic    = ABCDEFGHIJKLM & 0x0008;
//        uint8_t cQuickSaves = (ABCDEFGHIJKLM & 0x00F0) >> 4;
//        bool fEncrypted             = ABCDEFGHIJKLM & 0x0100;
        document.fWhichTblStm       = (ABCDEFGHIJKLM & 0x0200) >> 9;
//        bool fReadOnlyRecommended   = ABCDEFGHIJKLM & 0x0400;
//        bool fWriteReservation      = ABCDEFGHIJKLM & 0x0800;
//        bool fExtChar               = ABCDEFGHIJKLM & 0x1000;
//        bool fLoadOverwrite         = ABCDEFGHIJKLM & 0x2000;
//        bool fFarEast               = ABCDEFGHIJKLM & 0x4000;
//        bool fObfuscation           = ABCDEFGHIJKLM & 0x8000;
        document.fcClx = read_u32_le(stream + 0x01A2);
        document.lcbClx = read_u32_le(stream + 0x01A6);
        
        switch (nFib) {
            case 0x013F:
                document.docVersion = "Word 2007";
                break;
            case 0x0112:
                document.docVersion = "Word 2003";
                break;
            case 0x0101:
                document.docVersion = "Word 2002";
                break;
            case 0x00D9:
                document.docVersion = "Word 2000";
                break;
            case 0x00C1:
                document.docVersion = "Word 97";
                break;
            case 0x00C0:
                document.docVersion = "Word 97";
                break;
            case 0x00B0:
                document.docVersion = "Word 95";
                break;
            case 0x00A5:
                document.docVersion = "Word 6.0";
                break;
            default:
                break;
        }
    }
}

void read_ppt(Document& document, const uint8_t *stream, size_t stream_len, int codepage) {
    
    size_t offset = 0;
    
    while (offset + sizeof(RecordHeader) <= stream_len) {
        
        RecordHeader rh;
        rh = read_RecordHeader(stream + offset);
        offset += sizeof(RecordHeader);//+=8

        if(rh.recLen == 0) break;
        
        if (offset + rh.recLen > stream_len) break;

        switch (rh.recType) {
            case 0x0:
                return;
                break;
            case 0x0ff5: //RT_UserEditAtom (UserEditAtom)
//                std::cerr << "UserEditAtom" << "(" << rh.recLen << ")"  << std::endl;
                break;
            case 0x1772: //RT_PersistDirectoryAtom (PersistDirectoryAtom)
//                std::cerr << "PersistDirectoryAtom" << "(" << rh.recLen << ")"  << std::endl;
                break;
            case 0x03ee: //RT_Slide
//                std::cerr << "SlideContainer" << "(" << rh.recLen << ")"  << std::endl;
                break;
            case 0x03ef: //RT_SlideAtom
//                std::cerr << "SlideAtom" << "(" << rh.recLen << ")"  << std::endl;
                break;
            case 0x040c: //RT_Drawing
//                std::cerr << "DrawingContainer" << "(" << rh.recLen << ")"  << std::endl;
                break;
            case 0x0fc9 ://RT_Handout (HandoutContainer)
//                std::cerr << "HandoutContainer" << "(" << rh.recLen << ")"  << std::endl;
                break;
            case 0x03F0 ://RT_Notes (NotesContainer)
//                std::cerr << "NotesContainer" << "(" << rh.recLen << ")"  << std::endl;
                break;
            case 0x03f1 : //RT_NotesAtom (NotesAtom)
//                std::cerr << "NotesAtom" << "(" << rh.recLen << ")"  << std::endl;
                break;
            case 0x03F8 ://RT_MainMaster (MainMasterContainer)
//                std::cerr << "MainMasterContainer" << "(" << rh.recLen << ")"  << std::endl;
                break;
            case 0x1007 ://ExternalMciMovie (ExMCIMovieContainer)
//                std::cerr << "ExMCIMovieContainer" << "(" << rh.recLen << ")"  << std::endl;
                break;
            case 0x03E8 ://RT_Document (DocumentContainer)
            {
//                std::cerr << "DocumentContainer" << "(" << rh.recLen << ")"  << std::endl;
                                                
                DocumentAtom da;
                da = read_DocumentAtom(stream + offset);//RT_DocumentAtom
                                
                size_t da_offset = offset + sizeof(DocumentAtom);
                size_t pos = sizeof(DocumentAtom);
                
                while (pos + sizeof(RecordHeader) <= rh.recLen) {
                    
                    RecordHeader member;
                    member = read_RecordHeader(stream + da_offset);
                    da_offset += sizeof(RecordHeader);

                    pos += sizeof(RecordHeader);
                    
                    switch (member.recType) {
                        case 0x07d0 :
//                        std::cerr << "\tDocInfoListContainer" << "(" << member.recLen << ")"  << std::endl;
                        break;
                        case 0x03ea :
//                        std::cerr << "\tEndDocumentAtom" << "(" << member.recLen << ")"  << std::endl;
                        break;
                        case 0x0428 :
//                        std::cerr << "\tRoundTripCustomTableStyles12Atom" << "(" << member.recLen << ")"  << std::endl;
                        break;
                            case 0x0401 :
//                            std::cerr << "\tSlideShowDocInfoAtom" << "(" << member.recLen << ")"  << std::endl;
                            break;
                        case 0x0Fd9://HeadersFooters
//                            std::cerr << "\tHeadersFooters" << "(" << member.recLen << ")"  << std::endl;
                            break;
                        case 0x0FF0://SlideListWithText
//                            std::cerr << "\tSlideListWithTextContainer" << "(" << member.recLen << ")"  << std::endl;
                        {
//                            slideIndex++;
//                            std::cerr << "slide #" << slideIndex << std::endl;
                            Slide slide;
                            
                            size_t m_offset = da_offset;
                            size_t m_pos = sizeof(RecordHeader);
                            while (m_pos + sizeof(RecordHeader) <= member.recLen) {
                             
                                RecordHeader sl;
                                sl = read_RecordHeader(stream + m_offset);
                                m_pos += sizeof(RecordHeader);
                                m_offset += sizeof(RecordHeader);
                                
                                switch (sl.recType) {
                                    case 0x03f3 :
//                                        std::cerr << "\t\tSlidePersistAtom" << "(" << sl.recLen << ")"  << std::endl;
                                        break;
                                    case 0x0f9f :
//                                        std::cerr << "\t\tTextHeaderAtom" << "(" << sl.recLen << ")"  << std::endl;
                                        break;
                                    case 0x0fa9 :
//                                        std::cerr << "\t\tTextSpecialInfoDefaultAtom" << "(" << sl.recLen << ")"  << std::endl;
                                        break;
                                    case 0x0fa1 :
//                                        std::cerr << "\t\tStyleTextPropAtom" << "(" << sl.recLen << ")"  << std::endl;
                                        break;
                                    case 0x0faa :
//                                        std::cerr << "\t\tTextSpecialInfoAtom" << "(" << sl.recLen << ")"  << std::endl;
                                        break;
                                    case 0x0fa8 :
                                    {
//                                        std::cerr << "\t\tTextBytesAtom" << "(" << sl.recLen << ")"  << std::endl;
                                        std::string ansi = std::string((const char *)stream + m_offset, sl.recLen);
                                        std::string u8;

                                        ansi_to_utf8(ansi, u8, codepage);
//                                        std::cerr << u8 << std::endl;
//                                        slide.text.push_back(u8);
                                    }
                                        break;
                                    case 0x0fa0 :
//                                        std::cerr << "\t\tTextCharsAtom" << "(" << sl.recLen << ")"  << std::endl;
                                    {
                                        std::string u8;
                                        utf16_to_utf8((const uint8_t *)stream + m_offset, sl.recLen, u8);
                                        slide.text.push_back(u8);
//                                        std::cerr << u8 << std::endl;
                                    }
                                        break;
                                    default:
//                                        std::cerr << sl.recType << "(" << sl.recLen << ")"  << std::endl;
                                        break;
                                        
                                }
                                m_pos += sl.recLen;;
                                m_offset += sl.recLen;
                            }
                            
                            document.slides.push_back(slide);
                        }
                            break;
                        case 0x040B://DrawingGroup
//                            std::cerr << "\tDrawingGroup" << "(" << member.recLen << ")"  << std::endl;
                            break;
                        case 0x03F2://DocumentTextInfoContainer
//                            std::cerr << "\tDocumentTextInfoContainer" << "(" << member.recLen << ")"  << std::endl;
                            break;
                        default:
//                            std::cerr << member.recType << std::endl;
                            break;
                    }
                    
                    da_offset += member.recLen;
                    pos += member.recLen;
                }
                
            }
                break;
            default:
//                std::cerr << rh.recType << "(" << rh.recLen << ")"  << std::endl;
                continue;
                break;
        }
        
        offset += rh.recLen;
    }
}

bool starts_with(const std::string& str, const std::string& prefix) {
    return str.rfind(prefix, 0) == 0;
}

static void process_root(Document& document,
                           libolecf_item_t *root, int codepage) {
 
    libolecf_error_t *error = NULL;
    size_t utf8_string_size;
    
    Message message;
    Account sender;
    Account recipient;
    
    std::vector<std::string> properties_to_ignore = {
        "Pictures",
        "Current User",
        "EncryptedSummary",
        "_xmlsignatures",
        "_signatures",
        "\005SummaryInformation",
        "\005DocumentSummaryInformation",
        "\001CompObj",
        "__substg1.0_65E30102",
        "__substg1.0_65E20102",
        "__substg1.0_80090048",
        "__substg1.0_3FFB0102",
        "__substg1.0_30140102",
        "__substg1.0_0F030102",
        "__substg1.0_0C1D0102",
        "__substg1.0_0C190102",
        "__substg1.0_00520102",
        "__substg1.0_00510102",
        "__substg1.0_004F0102",
        "__substg1.0_00430102",
        "__substg1.0_00410102",
        "__substg1.0_003F0102",
        "__substg1.0_003B0102",
        "__substg1.0_800B0102",
        "__substg1.0_80080102",
        "__substg1.0_800D001F",
        "__substg1.0_800C001F",
        "__substg1.0_8008001F",
        "__substg1.0_8007001F",
        "__substg1.0_8003001F",
        "__substg1.0_8002001F",
        "__substg1.0_8001001F",
        "__substg1.0_4039001F",
        "__substg1.0_4038001F",
        "__substg1.0_4035001F",
        "__substg1.0_4034001F",
        "__substg1.0_4031001F",
        "__substg1.0_4030001F",
        "__substg1.0_4025001F",
        "__substg1.0_4024001F",
        "__substg1.0_4023001F",
        "__substg1.0_4022001F",
        "__substg1.0_300B0102"/*PidTagSearchKey*/,
        "__substg1.0_00710102"/*PidTagConversationIndex*/,
        "__substg1.0_80090102"/*PidTagAddressBookMember*/,
        "__substg1.0_0064001F"/*PidTagSentRepresentingAddressType*/,
        "__substg1.0_001A001F"/*PidTagMessageClass*/,
        "__substg1.0_0070001F"/*PidTagConversationTopic*/,
        "__substg1.0_8004001F"/*PidTagAddressBookFolderPathname*/,
        "__substg1.0_3FFA001F"/*PidTagLastModifierName*/,
        "__substg1.0_0075001F"/*PidTagReceivedByAddressType*/,
        "__substg1.0_0077001F"/*PidTagReceivedRepresentingAddressType*/,
        "__substg1.0_0050001F",
        "__substg1.0_0C1E001F"/*PidTagSenderAddressType*/,
        "__substg1.0_0E05001F",
        "__substg1.0_1015001F"/*PidTagBodyContentId*/,
        "__substg1.0_1035001F"/*PidTagInternetMessageId*/,
        "__substg1.0_800E001F"/*PidTagAddressBookReports*/
       };
    
    std::vector<std::string> sub_properties_to_ignore = {
        "__substg1.0_00020102",
        "__substg1.0_00030102",
        "__substg1.0_00040102",
        "__substg1.0_10010102",
        "__substg1.0_100F0102",
        "__substg1.0_10110102",
        "__substg1.0_101E0102",
        "__substg1.0_100A0102",
        "__substg1.0_10120102",
        "__substg1.0_10090102",
        "__substg1.0_10140102",
        "__substg1.0_10150102",
        "__substg1.0_10060102",
        "__substg1.0_3D010102",
        "__substg1.0_0FF60102",
        "__substg1.0_101C0102",
        "__substg1.0_10170102",
        "__substg1.0_10080102",
        "__substg1.0_10180102",
        "__substg1.0_10040102",
        "__substg1.0_100D0102",
        "__substg1.0_10190102",
        "__substg1.0_0C240102",
        "__substg1.0_3A0C001F",
        "__substg1.0_371D0102",
        "__substg1.0_3712001F",
        "__substg1.0_370E001F",
        "__substg1.0_3707001F",
        "__substg1.0_3704001F",
        "__substg1.0_3703001F",
        "__substg1.0_37010102",
        "__substg1.0_0FF90102",
        "__substg1.0_5FF70102",
        "__substg1.0_5FE5001F",
        "__substg1.0_3A20001F",
        "__substg1.0_39FE001F",
        "__substg1.0_300B0102",
        "__substg1.0_3003001F",
        "__substg1.0_3002001F",
        "__substg1.0_0FFF0102",
        "__substg1.0_0C250102"
    };
    
    std::vector<uint8_t>doc_0table_buf(0);
    std::vector<uint8_t>doc_1table_buf(0);
    std::vector<uint8_t>document_buf(0);
    
    int number_of_sub_items = 0;
    if(libolecf_item_get_number_of_sub_items(root, &number_of_sub_items, &error) == 1) {
        for (int i = 0; i < number_of_sub_items; ++i) {
            libolecf_item_t *sub_item = NULL;
            if(libolecf_item_get_sub_item(root, i, &sub_item, &error) == 1) {
                if(libolecf_item_get_utf8_name_size(sub_item, &utf8_string_size, &error) == 1) {
                    std::vector<uint8_t>property_buf(utf8_string_size + 1);
                    if(libolecf_item_get_utf8_name(sub_item, property_buf.data(), property_buf.size(), &error) == 1) {
                        std::string property = (const char *)property_buf.data();
                        uint32_t size = 0;
                        if(libolecf_item_get_size(sub_item, &size, &error) == 1) {
                            
                            if (document.type == "") {
                                if (starts_with(property, "__substg1.0_")) {
                                    document.type = "msg";
                                }
                                if (property == "PowerPoint Document") {
                                    document.type = "ppt";
                                }
                            }
                            
                            if(size == 0) {
                                goto read_sub_items;
                            }
                            
                            for (std::string& uninteresting : properties_to_ignore) {
                                if(property == uninteresting) goto read_sub_items;
                            }

                            std::vector<uint8_t>item_value_buf(size);
                            ssize_t len = libolecf_stream_read_buffer(sub_item, item_value_buf.data(), item_value_buf.size(), NULL);
                            
                            if(property == "WordDocument") {
                                if(len != -1) {
                                    read_doc(document, item_value_buf.data(), item_value_buf.size(), codepage);
                                    document_buf = item_value_buf;
                                }
                            }
                            
                            if (property == "0Table") {
                                doc_0table_buf = item_value_buf;
                            }
                            if (property == "1Table") {
                                doc_1table_buf = item_value_buf;
                            }
                            
                            if(property == "PowerPoint Document") {
                                if(len != -1) {
                                    read_ppt(document, item_value_buf.data(), item_value_buf.size(), codepage);
                                }
                                goto read_sub_items;
                            }

                            //PidTagTransportMessageHeaders
                            if(property == "__substg1.0_007D001F") {
                                if(len != -1) {
                                    std::string headers;
                                    utf16le_to_utf_8(item_value_buf, headers);
                                    message.headers = headers;
                                }
                                goto read_sub_items;
                            }
                            
                            //SENDER
                            
                            //PidTagSenderName
                            if(property == "__substg1.0_0C1A001F") {
                                if(len != -1) {
                                    std::string name;
                                    utf16le_to_utf_8(item_value_buf, name);
                                    sender.name = name;
                                }
                                goto read_sub_items;
                            }
                            //PidTagSentRepresentingName
                            if(property == "__substg1.0_0042001F") {
                                if(len != -1) {
                                    std::string name;
                                    utf16le_to_utf_8(item_value_buf, name);
                                    sender.name = name;
                                }
                                goto read_sub_items;
                            }
                            //PidTagSenderEmailAddress
                            if(property == "__substg1.0_0C1F001F") {
                                if(len != -1) {
                                    std::string address;
                                    utf16le_to_utf_8(item_value_buf, address);
                                    sender.address = address;
                                }
                                goto read_sub_items;
                            }
                            //PidTagSentRepresentingEmailAddress
                            if(property == "__substg1.0_0065001F") {
                                if(len != -1) {
                                    std::string address;
                                    utf16le_to_utf_8(item_value_buf, address);
                                    sender.address = address;
                                }
                                goto read_sub_items;
                            }

                            //RECIPIENT
                            
                            //PidTagEmailAddress
                            if(property == "__substg1.0_3003001F") {
                                if(len != -1) {
                                    std::string address;
                                    utf16le_to_utf_8(item_value_buf, address);
                                    recipient.address = address;
                                }
                                goto read_sub_items;
                            }
                            //PidTagReceivedRepresentingEmailAddress
                            if(property == "__substg1.0_0078001F") {
                                if(len != -1) {
                                    std::string address;
                                    utf16le_to_utf_8(item_value_buf, address);
                                    recipient.address = address;
                                }
                                goto read_sub_items;
                            }
                            //PidTagReceivedByEmailAddress
                            if(property == "__substg1.0_0076001F") {
                                if(len != -1) {
                                    std::string address;
                                    utf16le_to_utf_8(item_value_buf, address);
                                    recipient.address = address;
                                }
                                goto read_sub_items;
                            }
                            //PidTagReceivedByName
                            if(property == "__substg1.0_0040001F") {
                                if(len != -1) {
                                    std::string name;
                                    utf16le_to_utf_8(item_value_buf, name);
                                    recipient.name = name;
                                }
                                goto read_sub_items;
                            }
                            //PidTagReceivedRepresentingName
                            if(property == "__substg1.0_0044001F") {
                                if(len != -1) {
                                    std::string name;
                                    utf16le_to_utf_8(item_value_buf, name);
                                    recipient.name = name;
                                }
                                goto read_sub_items;
                            }

                            //PidTagDisplayTo
                            if(property == "__substg1.0_0E04001F") {
                                if(len != -1) {
                                    std::string name;
                                    utf16le_to_utf_8(item_value_buf, name);
                                    recipient.name = name;
                                }
                                goto read_sub_items;
                            }
     
                            //PidTagSubject:PT_UNICODE
                            if(property == "__substg1.0_0037001F") {
                                if(len != -1) {
                                    std::string subject;
                                    utf16le_to_utf_8(item_value_buf, subject);
                                    message.subject = subject;
                                }
                                goto read_sub_items;
                            }
                            
                            //PidTagSenderSmtpAddress
                            if(property == "__substg1.0_5D01001F") {
                                if(len != -1) {
                                    std::string address;
                                    utf16le_to_utf_8(item_value_buf, address);
                                    sender.address = address;
                                }
                                goto read_sub_items;
                            }
                            
                            //PidTagSentRepresentingSmtpAddress
                            if(property == "__substg1.0_5D02001F") {
                                if(len != -1) {
                                    std::string address;
                                    utf16le_to_utf_8(item_value_buf, address);
                                    sender.address = address;
                                }
                                goto read_sub_items;
                            }

                            //PidTagNormalizedSubject:PT_UNICODE
                            if(property == "__substg1.0_0E1D001F") {
                                if((len != -1) && (message.subject.length() == 0)) {
                                    std::string subject;
                                    utf16le_to_utf_8(item_value_buf, subject);
                                    message.subject = subject;
                                }
                                goto read_sub_items;
                            }
                            
                            //PidTagBody:PT_UNICODE
                            if(property == "__substg1.0_1000001F") {
                                if(len != -1) {
                                    std::string text;
                                    utf16le_to_utf_8(item_value_buf, text);
                                    message.text = text;
                                }
                                goto read_sub_items;
                            }
                            //PidTagRtfCompressed:PT_BINARY
                            if(property == "__substg1.0_10090102") {
                                if(len != -1) {
                                    /*
                                     https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-oxrtfcp/bf6f7e51-4939-4e16-9b44-6bd644e9ae5d
                                     */
                                    std::vector<uint8_t>rtf;
                                    if(process_rtf(item_value_buf, rtf) == 0) {
                                        message.rtf = std::string((const char *)rtf.data(), rtf.size());
                                    }
                                }
                                goto read_sub_items;
                            }
                            //PidTagHtml:PT_BINARY
                            if(property == "__substg1.0_10130102") {
                                if(len != -1) {
                                    message.html = std::string((const char *)item_value_buf.data(), item_value_buf.size());
                                }
                                goto read_sub_items;
                            }
                        }

//                        std::cerr << property << "(" << size << ")" << std::endl;
                        
                    read_sub_items:
                        
                        int number_of_sub_sub_items = 0;
                        if(libolecf_item_get_number_of_sub_items(sub_item, &number_of_sub_sub_items, &error) == 1) {
                            for (int j = 0; j < number_of_sub_sub_items; ++j) {
                                libolecf_item_t *sub_sub_item = NULL;
                                if(libolecf_item_get_sub_item(sub_item, j, &sub_sub_item, &error) == 1) {
                                    if(libolecf_item_get_utf8_name_size(sub_sub_item, &utf8_string_size, &error) == 1) {
                                        std::vector<uint8_t>buf(utf8_string_size + 1);
                                        if(libolecf_item_get_utf8_name(sub_sub_item, buf.data(), buf.size(), &error) == 1) {
                                            std::string property = (const char *)buf.data();

                                            uint32_t size = 0;
                                            if(libolecf_item_get_size(sub_sub_item, &size, &error) == 1) {

                                                if(size == 0) {
                                                    continue;
                                                }
                                                
                                                bool continue_outer = false;
                                                for (std::string& uninteresting : sub_properties_to_ignore) {
                                                    if(property == uninteresting) {
                                                        continue_outer = true;
                                                        break;
                                                    }
                                                }
                                                
                                                if(continue_outer){
                                                    continue;
                                                }
                                                
                                                std::vector<uint8_t>_buf(size);
                                                ssize_t len = libolecf_stream_read_buffer(sub_sub_item, _buf.data(), _buf.size(), NULL);
                                                
                                                //PidTagRecipientDisplayName:PT_UNICODE
                                                if(property == "__substg1.0_5FF6001F") {
                                                    if(len != -1) {
                                                        std::string name;
                                                        utf16le_to_utf_8(_buf, name);
                                                        recipient.name = name;
                                                    }
                                                    continue;
                                                }
                                                //PidTagDisplayName:PT_UNICODE
                                                if(property == "__substg1.0_3001001F") {
                                                    if(len != -1) {
                                                        std::string name;
                                                        utf16le_to_utf_8(_buf, name);
                                                        recipient.name = name;
                                                    }
                                                    continue;
                                                }
                                                
//                                                std::cerr << "\t" << property << "(" << size << ")" << std::endl;

                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (document.type == "doc") {
        
        std::vector<uint8_t> clx;
        if(document.fWhichTblStm == 1) {
            if(doc_1table_buf.size() >= (document.fcClx + document.lcbClx)) {
                clx.assign(
                           doc_1table_buf.begin() + document.fcClx,
                           doc_1table_buf.begin() + document.fcClx + document.lcbClx
                );
            }
        }else{
            if(doc_0table_buf.size() >= (document.fcClx + document.lcbClx)) {
                clx.assign(
                           doc_0table_buf.begin() + document.fcClx,
                           doc_0table_buf.begin() + document.fcClx + document.lcbClx
                );
            }
        }
        
        struct Piece {
            uint32_t cpStart;
            uint32_t cpEnd;
            uint32_t offset;
            bool fCompressed;
        };
        
        std::vector<Piece> pieces;
        
        // Find 0x02 byte marking the start of Piece Table
        auto it = std::find(clx.begin(), clx.end(), 0x02);
        if (it != clx.end()) {
            size_t offset = std::distance(clx.begin(), it);
            offset += sizeof(uint8_t);
            size_t lcb = read_u32_le(clx.data() + offset);
            offset += sizeof(uint32_t);
            size_t pcd_count = lcb / 12;
            size_t cp_count = pcd_count + 1;
            if(lcb == (pcd_count * 8) + (cp_count * 4)) {
                std::vector<uint32_t> cps(cp_count);
                for (size_t i = 0; i < cp_count; ++i) {
                    cps[i] = read_u32_le(clx.data() + offset);
                    offset += sizeof(uint32_t);
                }
                for (size_t i = 0; i < pcd_count; ++i) {
                    uint16_t flags = read_u16_le(clx.data() + offset);
                    offset += sizeof(uint16_t);
                    uint32_t fc = read_u32_le(clx.data() + offset);
                    offset += sizeof(uint32_t);
                    uint16_t prm0 = read_u16_le(clx.data() + offset);
                    offset += sizeof(uint16_t);
                    uint32_t file_offset = fc & 0x3FFFFFFF;
                    bool fCompressed = (fc >> 30) & 0x1;//actually, isCompressed, i.e. opposite of is_unicode
                    if(fCompressed) {
                        file_offset = file_offset/2;
                    }
                    Piece piece;
                    piece.fCompressed = fCompressed;
                    piece.cpStart = cps[i];
                    piece.cpEnd = cps[i + 1];
                    piece.offset = file_offset;
                    pieces.push_back(piece);
                }
//                std::cerr << "got " << pieces.size() << " pieces!" << std::endl;

                for (const auto& piece : pieces) {
                uint32_t charCount = piece.cpEnd - piece.cpStart;
                uint32_t byteCount = piece.fCompressed ? charCount : charCount * 2;
                if (piece.offset + byteCount > document_buf.size()) {
//                std::cerr << "Piece out of bounds\n";
                continue;
                }
                const uint8_t* dataPtr = document_buf.data() + piece.offset;
                    std::string u8;
                    if (piece.fCompressed) {
                        std::string ansi = std::string((const char*)dataPtr, byteCount);
                        ansi_to_utf8(ansi, u8, codepage);
                    } else {
                        utf16_to_utf8((const uint8_t *)dataPtr, byteCount, u8);
                    }
                    document.text += u8;
                }
            }
        }
    }
    
    message.sender = sender;
    message.recipient = recipient;
    document.message = message;
}

#ifdef _WIN32
static HWND create_rtf_window() {
    HWND hwnd = NULL;
    if (ghmodule)
    {
        hwnd = CreateWindowExW(
                               0, L"RichEdit20W", nullptr,
                               WS_CHILD | ES_MULTILINE,
                               0, 0, 0, 0,
                               HWND_MESSAGE, // message-only parent works with RichEdit 2.0
                               nullptr,
                               GetModuleHandle(nullptr),
                               nullptr);
    }
    return hwnd;
}
#endif

static void delete_rtf_window(HWND hwnd) {
#ifdef _WIN32
    if (hwnd) {
        DestroyWindow(hwnd);
    }
#endif
}

bool olecf_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                      output_type mode,
                      int max_paragraph_length,
                      bool unique_values_only,
                      bool text_as_tokens,
                      int tokens_length,
                      bool token_padding,
                      int pooling_mode,
                      float overlap_ratio,
                      int codepage) {
    
#if defined(_WIN32)
    std::wstring temp_input_path;
    HWND hwnd = create_rtf_window();
#else
    std::string  temp_input_path;
    HWND hwnd = NULL;
#endif
    
    bool success = false;
    
    if(!create_temp_file_path(temp_input_path)){
        FILE *f = _fopen(temp_input_path.c_str(), _wb);
        if(f) {
            fwrite(data.data(), 1, data.size(), f);
            fclose(f);
        }
    }
    
    libolecf_file_t *file = NULL;
    libolecf_error_t *error = NULL;
    
    Document document;

    if (libolecf_file_initialize(&file, &error) == 1) {
        if (_libolecf_file_open(file, temp_input_path.c_str(), LIBOLECF_OPEN_READ, &error) == 1) {
            document.type = "";
            libolecf_item_t *root = NULL;
            if (libolecf_file_get_root_item(file, &root, &error) == 1) {
                process_root(document, root, codepage);
                if(document.type == "msg"){
                    document_to_json_msg(document,
                                         obj,
                                         mode,
                                         max_paragraph_length,
                                         unique_values_only,
                                         text_as_tokens,
                                         tokens_length,
                                         token_padding,
                                         pooling_mode,
                                         overlap_ratio,
                                         hwnd);
                    success = true;
                }
                if(document.type == "ppt"){
                    document_to_json_ppt(document,
                                         obj,
                                         mode,
                                         max_paragraph_length,
                                         unique_values_only,
                                         text_as_tokens,
                                         tokens_length,
                                         token_padding,
                                         pooling_mode,
                                         overlap_ratio,
                                         hwnd);
                    success = true;
                }
                if(document.type == "doc"){
                    document_to_json_doc(document,
                                         obj,
                                         mode,
                                         max_paragraph_length,
                                         unique_values_only,
                                         text_as_tokens,
                                         tokens_length,
                                         token_padding,
                                         pooling_mode,
                                         overlap_ratio,
                                         hwnd);
                    success = true;
                }
            }else{
                std::cerr << "Failed to get MSG root item!" << std::endl;
            }
        }else{
            std::cerr << "Failed to load MSG file!" << std::endl;
        }
        libolecf_file_free(&file, &error);
    }
    
    ob_set_b(obj, L"success", success);
    
    if(!success) {
        ob_set_a(obj, L"type", L"unknown");
    }
    
    delete_rtf_window(hwnd);

    if(temp_input_path.length()) {
#ifdef _WIN32
        DeleteFile((LPCWSTR)temp_input_path.c_str());
#else
        _unlink(temp_input_path.c_str());
#endif
    }
    
    return success;
}
