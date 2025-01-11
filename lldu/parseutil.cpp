//-------------------------------------------------------------------------------------------------
// File: parseutil.cpp
// Author: Dennis Lang
//
// Desc: Parsing utility functions.
//
//-------------------------------------------------------------------------------------------------
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
        lstring valueStr(value);
        convertSpecialChar(valueStr);
        return ignoreCase ? std::regex(valueStr, regex_constants::icase) : std::regex(valueStr);
    } catch (const std::regex_error& regEx) {
        Colors::showError("Invalid regular expression ", regEx.what(), ", Pattern=", value);
    } catch (...) {
        Colors::showError("Invalid regular expression=", value);
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

    if (strncasecmp(validCmd, possibleCmd, std::min(validLen, possibleLen)) == 0) {
        parseArgSet.insert(validCmd);
        return true;
    }

    if (reportErr) {
        std::cerr << Colors::colorize("_R_Unknown option:'") << possibleCmd << "', expect:'" << validCmd << Colors::colorize("'_X_\n");
        optionErrCnt++;
    }
    return false;
}

//-------------------------------------------------------------------------------------------------
bool ParseUtil::validPattern(PatternList& outList, lstring& value, const char* validCmd, const char* possibleCmd, bool reportErr) {
    bool isOk = validOption(validCmd, possibleCmd, reportErr);
    if (isOk) {
        if (!unixRegEx) {
            // Convert simple DOS patterns to regular expression
            //  .   -> [.]    // match on dot
            //  *   ->  .*    // zero or more of anything
            //  ?   ->  .     // single any character
            ReplaceAll(value, std::regex("([^[])[.]"), "$1[.]");
            ReplaceAll(value, "*", ".*");
            ReplaceAll(value, "?", ".");
        }
        outList.push_back(getRegEx(value));
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
            Colors::showError("Failed to open ", validCmd, " ", value, " ", strerror(err));
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
                Colors::showError("Warning: unrecognized escape sequence:", inPtr);
                throw( "Warning: unrecognized escape sequence" );
            case '\0': // Trailing slash
                inPtr--;
                break;
            case '\\':      // Double slash becomes single
                *outPtr++ = *inPtr;
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

//-------------------------------------------------------------------------------------------------
// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
// [static]
std::string& ParseUtil::fmtDateTime(string& outTmStr, time_t& now) {
    now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
    outTmStr = buf;
    return outTmStr;
}

//-------------------------------------------------------------------------------------------------
// [static]
lstring& ParseUtil::getParts(
        lstring& outPart,
        const char* partSelector,
        const char* name,       // just name, not extension
        const char* ext,        // just extension, no dot prefix
        unsigned num) {

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
#ifdef HAVE_WIN
#define byte win_byte_override      // Fix for c++ v17+
#include <Windows.h>
#undef byte                         // Fix for c++ v17+
#include <stdio.h>
#endif

#define RED    "\033[01;31m"
#define GREEN  "\033[01;32m"
#define YELLOW "\033[01;33m"
#define BLUE   "\033[01;34m"
#define PINK   "\033[01;35m"
#define LBLUE  "\033[01;36m"
#define WHITE  "\033[01;37m"
#define OFF    "\033[00m"


string Colors::colorize(const char* inStr) {
#ifdef HAVE_WIN
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
    string str(inStr);

    // _x_  where x lowercase, colorize following word
    replaceRE(str, "_y_(\\w+)", YELLOW "$1" OFF);
    replaceRE(str, "_r_(\\w+)", RED "$1" OFF);
    replaceRE(str, "_g_(\\w+)", GREEN "$1" OFF);
    replaceRE(str, "_p_(\\w+)", PINK "$1" OFF);
    replaceRE(str, "_lb_(\\w+)", LBLUE "$1" OFF);
    replaceRE(str, "_w_(\\w+)", WHITE "$1" OFF);

    // _X_  where X uppercase, colorize until _X_
    replaceRE(str, "_Y_", YELLOW);
    replaceRE(str, "_R_", RED);
    replaceRE(str, "_G_", GREEN);
    replaceRE(str, "_P_", PINK);
    replaceRE(str, "_B_", BLUE);
    replaceRE(str, "_LB_", LBLUE);
    replaceRE(str, "_W_", WHITE);
    replaceRE(str, "_X_", OFF);
    return str;
}
