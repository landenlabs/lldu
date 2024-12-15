//-------------------------------------------------------------------------------------------------
// File: parseutil.cpp
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
#else
    #include <signal.h>
#endif

typedef unsigned int uint;

volatile bool abortFlag = false;    // Set true by signal handler


#ifdef HAVE_WIN
//-------------------------------------------------------------------------------------------------
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType)  {
    case CTRL_C_EVENT:  // Handle the CTRL-C signal.
        abortFlag = true;
        std::cerr << "\nCaught signal " << std::endl;
        Beep(750, 300);
        return TRUE;
    }

    return FALSE;
}

#else
//-------------------------------------------------------------------------------------------------
void sigHandler(int /* sig_t */ s) {
    abortFlag = true;
    std::cerr << "\nCaught signal " << std::endl;
}
#endif

// ---------------------------------------------------------------------------
ParseUtil::ParseUtil() noexcept {
#ifdef HAVE_WIN
    if (! SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
        std::cerr << "Failed to install sig handler" << endl;
    }
#else
    // signal(SIGINT, sigHandler);

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = sigHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    if (sigaction(SIGINT, &sigIntHandler, NULL) != 0) {
        std::cerr << "Failed to install sig handler" << endl;
    }
#endif
}

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
        // dumpStr("From", valueStr);
        return std::regex(valueStr);
        // return std::regex(valueStr, regex_constants::icase);
    } catch (const std::regex_error& regEx) {
        Colors::showError("Invalid regular expression ", regEx.what(), ", Pattern=", value);
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

    if (strncasecmp(validCmd, possibleCmd, std::min(validLen, possibleLen)) == 0)
        return true;

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
