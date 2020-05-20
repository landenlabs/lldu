//-------------------------------------------------------------------------------------------------
//
//  lldu      May-2020       Dennis Lang
//
//  Directory (disk) used space
//
//
//-------------------------------------------------------------------------------------------------
//
// Author: Dennis Lang - 2020
// http://landenlabs.com/
//
// This file is part of lldu project.
//
// ----- License ----
//
// Copyright (c) 2020 Dennis Lang
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

// 4291 - No matching operator delete found
#pragma warning( disable : 4291 )
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <ctype.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <map>
#include <algorithm>
#include <regex>
#include <exception>
 

// Project files
#include "ll_stdhdr.h"
#include "directory.h"
#include "split.h"


using namespace std;

// Helper types
typedef std::vector<std::regex> PatternList;
typedef unsigned int uint;
typedef std::vector<std::string> StringList;

struct PickPat {
    std::regex fromPat;
    std::string toStr;
};
typedef std::vector<PickPat> PickPatList;

// Runtime options
static PatternList includeFilePatList;
static PatternList excludeFilePatList;
static PickPatList pickPatList;
static StringList fileDirList;
static bool showFile = true;
static bool verbose = false;

static uint optionErrCnt = 0;
static uint patternErrCnt = 0;

struct DuInfo {
    size_t count;
    size_t diskSize;
    DuInfo() : count(0), diskSize(0) {}
};

typedef std::map<std::string, DuInfo> DuList;
DuList duList;

std::string separator = "\t";
std::string format = "%8.8e\t%8c\t%10s\n";        // %s\t%8d\t%10d\n";
std::string header = "Ext\tCount\tSize\n";

#ifdef WIN32
const char SLASH_CHAR('\\');
#include <assert.h>
#define strncasecmp _strnicmp
#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#else
const char SLASH_CHAR('/');
#endif
const char EXT_CHAR('.');

// ---------------------------------------------------------------------------
// Extract name part from path.
static 
lstring& getName(lstring& outName, const lstring& inPath)
{
    size_t nameStart = inPath.rfind(SLASH_CHAR) + 1;
    if (nameStart == 0)
        outName = inPath;
    else
        outName = inPath.substr(nameStart);
    return outName;
}

// ---------------------------------------------------------------------------
// Extract name part from path.
static
lstring& getExt(lstring& outExt, const lstring& inPath)
{
    size_t extPos = inPath.rfind(EXT_CHAR);
    if (extPos == std::string::npos)
        outExt = "";
    else
        outExt = inPath.substr(extPos+1);
    return outExt;
}

// ---------------------------------------------------------------------------
// Return true if inName matches pattern in patternList
static 
bool FileMatches(const lstring& inName, const PatternList& patternList, bool emptyResult)
{
    if (patternList.empty() || inName.empty())
        return emptyResult;
    
    for (size_t idx = 0; idx != patternList.size(); idx++)
        if (std::regex_match(inName.begin(), inName.end(), patternList[idx]))
            return true;
    
    return false;
}


// ---------------------------------------------------------------------------
// Open, read and parse file.
static
bool ExamineFile(const lstring& filepath, const lstring& filename)
{
    struct stat filestat;
    if (stat(filepath, &filestat) != 0)
        return false;
    
    lstring ext;
    if (pickPatList.empty()) {
         getExt(ext, filename);
    } else  {
        std::smatch smatch;
        PickPatList::const_iterator iter;
        for (iter = pickPatList.cbegin(); iter != pickPatList.cend(); iter++) {
            regex_constants::match_flag_type flags = regex_constants::match_default;
            // tmpName = regex_replace(tmpName, iter->fromPat, iter->toStr, flags);
 
            if (std::regex_match(filename, smatch, iter->fromPat)) {
                // size_t pos = smatch.position();
                // size_t len = smatch[0].length();
                ext = regex_replace(filename, iter->fromPat, iter->toStr, flags);
                break;
            }
        }
    }
    
    DuInfo& duInfo = duList[ext];
    duInfo.count++;
    duInfo.diskSize += filestat.st_size;
    
    return false;
}


// ---------------------------------------------------------------------------
// Locate matching files which are not in exclude list.
static 
size_t FindFile(const lstring& fullname)
{
    size_t fileCount = 0;
    lstring name;
    getName(name, fullname);
    
    if (!name.empty()
        && !FileMatches(name, excludeFilePatList, false)
        && FileMatches(name, includeFilePatList, true))
    {
        if (ExamineFile(fullname, name))
        {
            fileCount++;
            if (showFile)
                std::cout << fullname << std::endl;
        }
    }
    
    return fileCount;
}

// ---------------------------------------------------------------------------
// Recurse over directories, locate files.
static 
size_t FindFiles(const lstring& dirname)
{
    Directory_files directory(dirname);
    lstring fullname;
    
    size_t fileCount = 0;
    
    struct stat filestat;
    try {
        if (stat(dirname, &filestat) == 0 && S_ISREG(filestat.st_mode))
        {
            fileCount += FindFile(dirname);
        }
    }
    catch (exception ex)
    {
        // Probably a pattern, let directory scan do its magic.
    }
    
    while (directory.more())
    {
        directory.fullName(fullname);
        if (directory.is_directory())
        {
            fileCount += FindFiles(fullname);
        }
        else if (fullname.length() > 0)
        {
            fileCount += FindFile(fullname);
        }
    }
    
    return fileCount;
}

// ---------------------------------------------------------------------------
// Return compiled regular expression from text.
static
std::regex getRegEx(const char* value)
{
    try {
        std::string valueStr(value);
        return std::regex(valueStr);
        // return std::regex(valueStr, regex_constants::icase);
    }
    catch (const std::regex_error& regEx)
    {
        std::cerr << regEx.what() << ", Pattern=" << value << std::endl;
    }
    
    patternErrCnt++;
    return std::regex("");
}

// ---------------------------------------------------------------------------
// Validate option matchs and optionally report problem to user.
static
bool ValidOption(const char* validCmd, const char* possibleCmd, bool reportErr = true)
{
    // Starts with validCmd else mark error
    size_t validLen = strlen(validCmd);
    size_t possibleLen = strlen(possibleCmd);
    
    if ( strncasecmp(validCmd, possibleCmd, std::min(validLen, possibleLen)) == 0)
        return true;
    
    if (reportErr)
    {
        std::cerr << "Unknown option:'" << possibleCmd << "', expect:'" << validCmd << "'\n";
        optionErrCnt++;
    }
    return false;
}

// ---------------------------------------------------------------------------
// replace=<fromPat>;<toText>
void addPicker(const char* replaceArg)
{
    Split parts(replaceArg, ";");
    if (parts.size() == 2)
    {
        PickPat pickPat;
        pickPat.fromPat = std::regex(parts[0]);
        pickPat.toStr = parts[1];
        pickPatList.push_back(pickPat);
    }
}

// ---------------------------------------------------------------------------
// Convert special characters from text to binary.
static const char* convertSpecialChar(const char* inPtr)
{
    uint len = 0;
    int x, n;
    const char* begPtr = inPtr;
    char* outPtr = (char*)inPtr;
    while (*inPtr)
    {
        if (*inPtr == '\\')
        {
            inPtr++;
            switch (*inPtr)
            {
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
                    sscanf(inPtr,"%3o%n",&x,&n);
                    inPtr += n-1;
                    *outPtr++ = (char)x;
                    break;
                case 'x':                                // hexadecimal
                    sscanf(inPtr+1,"%2x%n",&x,&n);
                    if (n>0)
                    {
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
        }
        else
            *outPtr++ = *inPtr++;
        len++;
    }
    
    *outPtr = '\0';
    return begPtr;
}

// ---------------------------------------------------------------------------
void printParts(
        const char* customFmt,
        const char* name,
        size_t count,
        size_t size)
{
    // Handle custom printf syntax to get to path parts:
    //    %#.#s
    //   n=name c=count, s=size
    
    const int NONE = 12345;
    lstring itemFmt;
    
    char* fmt = (char*)customFmt;
    while (*fmt) {
        char c = *fmt;
        if (c != '%') {
            putchar(c);
            fmt++;
        } else {
            const char* begFmt = fmt;
            int precision = NONE;
            int width = (int)strtol(fmt+1, &fmt, 10);
            if (*fmt == '.') {
                precision = (int)strtol(fmt+1, &fmt, 10);
            }
            c = *fmt;
        
            itemFmt = begFmt;
            itemFmt.resize(fmt - begFmt);
           
            switch (c) {
                case 'e':   // Extension
                case 'n':   // name
                    itemFmt += "s";
                    printf(itemFmt, name);
                    break;
                case 'c':   // Count
                    itemFmt += "lu";    // unsigned long formatter
                    printf(itemFmt, count);
                    break;
                case 's':   // Size
                    itemFmt += "lu";    // unsigned long formatter
                    printf(itemFmt, size);
                    break;
                default:
                    putchar(c);
                    break;
            }
            fmt++;
        }
    }
}

// ---------------------------------------------------------------------------
void help(const char* arg0) {
    cerr << "\n" << arg0 << "  Dennis Lang v1.1 (LandenLabs.com) " __DATE__ << "\n"
    << "\nDes: Directory (disk) used space inventory \n"
    "Use: lldu [options] directories...   or  files\n"
    "\n"
    " Options (only first unique characters required, options can be repeated):\n"
    "   -includefile=<filePattern>\n"
    "   -excludefile=<filePattern>\n"
    "   -verbose\n"
    "   -pick=<fromPat>;<toStr>         ; Def: [^.]*[.](.+);$1 \n"
    "   -format=<format-3-values>       ; Def: %e\\t%c\\t%s\\n \n"
    "        e=ext, c=count, s=size\n"
    "   -header=<header>                ; Def: Ext\\tCount\\tSize\\n \n"
    "\n"
    " Example:\n"
    "   lldu '-inc=*.bak' -ex=foo.json '-ex=*/subdir2' dir1/subdir dir2 *.txt file2.json \n"
    "   lldu '-exclude=\\.*' '-pick=[^.]+[.](.{4,});other' . \n"
    "\n"
    "  Output:\n"
    "    Ext  Count  Size\n"
    "    ext1 count1 size1 \n"
    "    ext2 count2 size2 \n"
    "\n";
}

// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{  
    if (argc == 1)
    {
        help(argv[0]);
        return 0;
    }
    else
    {
        bool doParseCmds = true;
        string endCmds = "--";
        for (int argn = 1; argn < argc; argn++)
        {
            if (*argv[argn] == '-' && doParseCmds)
            {
                lstring argStr(argv[argn]);
                Split cmdValue(argStr, "=", 2);
                if (cmdValue.size() == 2)
                {
                    lstring cmd = cmdValue[0];
                    lstring value = cmdValue[1];

                    switch (cmd[(unsigned)1])
                    {
                        case 'i':   // includeFile=<pat>
                            if (ValidOption("includefile", cmd+1))
                            {
                                ReplaceAll(value, "*", ".*");
                                includeFilePatList.push_back(getRegEx(value));
                            }
                            break;
                        case 'e':   // excludeFile=<pat>
                            if (ValidOption("excludefile", cmd+1))
                            {
                                ReplaceAll(value, "*", ".*");
                                excludeFilePatList.push_back(getRegEx(value));
                            }
                            break;
                            
                        case 'p': // pick=<fromPat>;<toText>
                            if (ValidOption("pick", cmd+1))
                            {
                                addPicker(convertSpecialChar(value));
                            }
                            break;
                            
                        case 'f':   // format=<str>
                            if (ValidOption("format", cmd+1))
                            {
                                format = convertSpecialChar(value);
                            }
                            break;
                        case 'h':   // header=<str>
                            if (ValidOption("header", cmd+1))
                            {
                                header = convertSpecialChar(value);
                            }
                            break;
                        case 's':   // separator=<str>
                            if (ValidOption("separator", cmd+1))
                            {
                                separator = convertSpecialChar(value);
                            }
                            break;
                            
                        default:
                            std::cerr << "Unknown command " << cmd << std::endl;
                            optionErrCnt++;
                            break;
                    }
                } else {
                    switch (argStr[(unsigned)1]) {
                        case 'v':   // -v=true or -v=anyThing
                            verbose = true;
                            continue;
                    }
                    
                    if (endCmds == argv[argn]) {
                        doParseCmds = false;
                    } else {
                        std::cerr << "Unknown command " << argStr << std::endl;
                        optionErrCnt++;
                    }
                }
            }
            else
            {
                // Store file directories
                fileDirList.push_back(argv[argn]);
            }
        }
        
        // if (pickPatList.empty()) {
            addPicker("[^.]*[.](.+);$1");
        //}
        
        if (patternErrCnt == 0 && optionErrCnt == 0 && fileDirList.size() != 0)
        {
            if (fileDirList.size() == 1 && fileDirList[0] == "-") {
                string filePath;
                while (std::getline(std::cin, filePath)) {
                    FindFiles(filePath);
                }
            } else {
                for (auto const& filePath : fileDirList)
                {
                    FindFiles(filePath);
                }
            }
        } else {
            help(argv[0]);
            std::cerr << std::endl;
            return -1;
       }
    }
    
    size_t totalCount = 0;
    size_t totalSize = 0;
    
    DuList::const_iterator iter;

    printf(header.c_str());
    for (iter= duList.cbegin(); iter != duList.cend(); iter++) {
        if (format.length() > 0) {
            printParts(format.c_str(), iter->first.c_str(), iter->second.count, iter->second.diskSize);
        } else {
            // std::cout << iter->first << separator << iter->second.count << separator << iter->second.diskSize << std::endl;
        }
        totalCount += iter->second.count;
        totalSize += iter->second.diskSize;
    }
    if (format.length() > 0) {
        printParts(format.c_str(), "Total", totalCount, totalSize);
    } else {
        // std::cout << iter->first << separator << iter->second.count << separator << iter->second.diskSize << std::endl;
    }
    
    return 0;
}
