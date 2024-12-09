//-------------------------------------------------------------------------------------------------
// File: parseutil.hpp
// Author: Dennis Lang
//
// Desc: Parsing utility functions.
//
//-------------------------------------------------------------------------------------------------
//
// Author: Dennis Lang - 2024
// https://landenlabs.com
//
// This file is part of lldupdir project.
//
// ----- License ----
//
// Copyright (c) 2024  Dennis Lang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "parseutil.hpp"

#include <assert.h>
#include <iostream>
#include <fstream>
#include <regex>

#ifdef HAVE_WIN
#define strncasecmp _strnicmp
#endif

typedef unsigned int uint;

// ---------------------------------------------------------------------------
void ParseUtil::showUnknown(const char* argStr) {
    std::cerr << Colors::colorize("Use -h for help.\n_Y_Unknown option _R_") << argStr << Colors::colorize("_X_\n");
    optionErrCnt++;
}

// ---------------------------------------------------------------------------
// Return compiled regular expression from text.
std::regex ParseUtil::getRegEx(const char* value) {
    try {
        std::string valueStr(value);
        return std::regex(valueStr);
        // return std::regex(valueStr, regex_constants::icase);
    }  catch (const std::regex_error& regEx)   {
        std::cerr << Colors::colorize("_R") << regEx.what() << ", Pattern=" << value << Colors::colorize("_X_\n");
    }

    patternErrCnt++;
    return std::regex("");
}

//-------------------------------------------------------------------------------------------------
// Validate option matches and optionally report problem to user.
bool ParseUtil::validOption(const char* validCmd, const char* possibleCmd, bool reportErr) {
    // Starts with validCmd else mark error
    size_t validLen = strlen(validCmd);
    size_t possibleLen = strlen(possibleCmd);

    if ( strncasecmp(validCmd, possibleCmd, std::min(validLen, possibleLen)) == 0)
        return true;

    if (reportErr) {
        std::cerr << Colors::colorize("_R_Unknown option:'")  << possibleCmd << "', expect:'" << validCmd << Colors::colorize("'_X_\n");
        optionErrCnt++;
    }
    return false;
}

//-------------------------------------------------------------------------------------------------
bool ParseUtil::validPattern(PatternList& outList, lstring& value, const char* validCmd, const char* possibleCmd, bool reportErr) {
    bool isOk = validOption(validCmd, possibleCmd, reportErr);
    if (isOk)  {
        ReplaceAll(value, "*", ".*");
        ReplaceAll(value, "?", ".");
        outList.push_back(getRegEx(value));
        return true;
    }
    return isOk;
}

//-------------------------------------------------------------------------------------------------
bool ParseUtil::validFile(
          std::fstream& stream,
          int mode,
          const lstring& value,
          const char* validCmd,
          const char* possibleCmd,
          bool reportErr) {
    bool isOk = validOption(validCmd, possibleCmd, reportErr);
    if (isOk) {
        stream.open(value, mode);
        int err = errno;
        if (stream.bad()) {
            std::cerr << Colors::colorize("_R_Failed to open ") << validCmd << " "  << value << " " << strerror(err) <<  Colors::colorize("'_X_\n");
            optionErrCnt++;
        }
    }
    return isOk;
}


// ---------------------------------------------------------------------------
// Return true if inName matches pattern in patternList
// [static]
bool ParseUtil::FileMatches(const lstring& inName, const PatternList& patternList, bool emptyResult) {
    if (patternList.empty() || inName.empty())
        return emptyResult;

    for (size_t idx = 0; idx != patternList.size(); idx++)
        if (std::regex_match(inName.begin(), inName.end(), patternList[idx]))
            return true;

    return false;
}

//-------------------------------------------------------------------------------------------------
// Convert special characters from text to binary.
// [static]
const char* ParseUtil::convertSpecialChar(const char* inPtr) {
    uint len = 0;
    int x, n, scnt;
    const char* begPtr = inPtr;
    char* outPtr = (char*)inPtr;
    while (*inPtr) {
        if (*inPtr == '\\') {
            inPtr++;
            switch (*inPtr) {
            case 'n': *outPtr++ = '\n'; break;
            case 't': *outPtr++ = '\t'; break;
            case 'v': *outPtr++ = '\v'; break;
            case 'b': *outPtr++ = '\b'; break;
            case 'r': *outPtr++ = '\r'; break;
            case 'f': *outPtr++ = '\f'; break;
            case 'a': *outPtr++ = '\a'; break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                scnt = sscanf(inPtr, "%3o%n", &x, &n);  // "\010" octal sets x=8 and n=3 (characters)
                assert(scnt == 1);
                inPtr += n - 1;
                *outPtr++ = (char)x;
                break;
            case 'x':                                // hexadecimal
                scnt = sscanf(inPtr + 1, "%2x%n", &x, &n);  // "\1c" hex sets x=28 and n=2 (characters)
                assert(scnt == 1);
                if (n > 0) {
                    inPtr += n;
                    *outPtr++ = (char)x;
                    break;
                }
            // seep through
            default:
                throw( "Warning: unrecognized escape sequence" );
            case '\\':
            case '\?':
            case '\'':
            case '\"':
                *outPtr++ = *inPtr;
                break;
            }
            inPtr++;
        } else
            *outPtr++ = *inPtr++;
        len++;
    }

    *outPtr = '\0';
    return begPtr;
}

// ---------------------------------------------------------------------------
#include <locale>
#include <iostream>

template <typename Ch>
class numfmt: public std::numpunct<Ch> {
    int group;    // Number of characters in a group
    Ch  separator; // Character to separate groups
public:
    numfmt(Ch sep, int grp): separator(sep), group(grp) {}
private:
    Ch do_thousands_sep() const { return separator; }
    std::string do_grouping() const { return std::string(1, group); }
};

inline void EnableCommaCout() {
#if 0
    char sep = ',';
    int group = 3;
    std::cout.imbue(std::locale(std::locale(),
        new numfmt<char>(sep, group)));
#else
    setlocale(LC_ALL, "");
    std::locale mylocale("");   // Get system locale
    std::cout.imbue(mylocale);
#endif
}

inline void DisableCommaCout() {
    std::cout.imbue(std::locale(std::locale()));
}

#if defined(__APPLE__) && defined(__MACH__)
    #include <printf.h>
    #define PRINTF(a,b) xprintf(_domain, NULL, a,b)
    static printf_domain_t _domain;
#else
    #define PRINTF(a,b) printf(a,b)
#endif

inline void initPrintf() {
#if defined(__APPLE__) && defined(__MACH__)
    _domain = new_printf_domain();
    setlocale(LC_ALL, "en_US.UTF-8");
#endif
}



#if 0
// ---------------------------------------------------------------------------
const char* fmtNumComma(size_t n, char*& buf) {
    char* result = buf;
    /*
     // if n is signed value.
    if (n < 0) {
        *buf++ = "-";
        return printfcomma(-n, buf);
    }
    */
    if (n < 1000) {
        buf += snprintf((char*)buf, 4, "%lu", (unsigned long)n);
        return result;
    }
    fmtNumComma(n / 1000, buf);
    buf += snprintf((char*)buf, 5, ",%03lu", (unsigned long) n % 1000);
    return result;
}

// Legacy ?windows? way of adding commas
char buf[40];
printf("%s", fmtNumComma(value, buf));
#endif

/*
#if defined(__APPLE__) && defined(__MACH__)
#include <printf.h>
#define PRINTF(a,b) xprintf(domain, NULL, a,b)
#else
#define PRINTF(a,b) printf(a,b)
#endif
*/

//-------------------------------------------------------------------------------------------------
// [static]
lstring& ParseUtil::getParts(
        lstring& outPart,
        const char* partSelector,
        const char* name,       // just name, not extension
        const char* ext,        // just extension, no dot prefix
        unsigned num) {

#ifdef HAVE_WIN
    const char* FMT_NUM = "lu";
#else
    const char* FMT_NUM = "'lu";  //  "`lu" linux supports ` to add commas
    EnableCommaCout();
#endif
    
    unsigned width=0;
    char numFmt[10], numStr[10];
    
    const char* fmt = partSelector;
    while (*fmt) {
        char c = *fmt;
        if (c == '\'' || c == '"') {
            while (*++fmt != c)
                outPart += *fmt;
            fmt++;
        } else {
            switch (c) {
            case 'E':   // Extension
                outPart += ext;
                break;
            case 'N':   // name
                outPart += name;
                break;
            case '#':   // Size
                width = 1;
                while (*fmt++ == '#')
                    width++;
                fmt -= 2;
                snprintf(numFmt, sizeof(numFmt), "%%0%uu", width);
                snprintf(numStr, sizeof(numStr), numFmt, num);
                outPart += numStr;
                break;
            default:
                outPart += *fmt;
                break;
            }
            fmt++;
        }
    }
    
    return outPart;
}

//-------------------------------------------------------------------------------------------------
void ParseUtil::printParts(
    const char* customFmt,
    const char* name,
    size_t count,
    size_t links,
    size_t size) {
    /*
    #if defined(__APPLE__) && defined(__MACH__)
        printf_domain_t domain = new_printf_domain();
        setlocale(LC_ALL, "en_US.UTF-8");
    #endif
     */
    initPrintf();

    // Handle custom printf syntax to get to path parts:
    //    %#.#s
    //   n=name c=count, s=size

    const int NONE = 12345;
    lstring itemFmt;
#ifdef HAVE_WIN
    const char* FMT_NUM = "lu";
#else
    const char* FMT_NUM = "'lu";  //  "`lu" linux supports ` to add commas
#endif
 
    char* fmt = (char*)customFmt;
    while (*fmt) {
        char c = *fmt;
        if (c != '%') {
            putchar(c);
            fmt++;
        } else {
            const char* begFmt = fmt;
            int precision = NONE;
            int width = (int)strtol(fmt + 1, &fmt, 10);
            if (*fmt == '.') {
                precision = (int)strtol(fmt + 1, &fmt, 10);
            }
            c = *fmt;

            itemFmt = begFmt;
            itemFmt.resize(fmt - begFmt);
            EnableCommaCout();

            switch (c) {
            case 'e':   // Extension
            case 'n':   // name
                itemFmt += "s";
                printf(itemFmt, name);
                break;
            case 'C':   // Count
                // DisableCommaCout();
                itemFmt += "lu";        // unsigned long formatter
                printf(itemFmt, count); // print with commas
                break;
            case 'c':   // Count
                itemFmt += FMT_NUM;       // unsigned long formatter
                PRINTF(itemFmt, count); // print with commas
                break;
            case 'L':   // Links
                // DisableCommaCout();
                itemFmt += "lu";        // unsigned long formatter
                printf(itemFmt, links);
                break;
            case 'l':   // Links
                itemFmt += FMT_NUM;       // unsigned long formatter
                PRINTF(itemFmt, links);
                break;
            case 'S':   // Size
                // DisableCommaCout();
                itemFmt += "lu";       // unsigned long formatter
                printf(itemFmt, size);
                break;
            case 's':   // Size
                itemFmt += FMT_NUM;       // unsigned long formatter
                PRINTF(itemFmt, size);
                break;

            default:
                putchar(c);
                break;
            }
            fmt++;
        }
    }
}
