//-------------------------------------------------------------------------------------------------
//
//  lldu      May-2020       Dennis Lang
//
//  Directory (disk) used space
//
//  TODO -
//    * Add option to do tree comparison
//       lldu -compare  dir1 dir2
//
//-------------------------------------------------------------------------------------------------
//
// Author: Dennis Lang - 2024
// http://landenlabs.com/
//
// This file is part of lldu project.
//
// ----- License ----
//
// Copyright (c) 2024 Dennis Lang
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

// Project files
#include "ll_stdhdr.hpp"
#include "directory.hpp"
#include "split.hpp"
#include "colors.hpp"
#include "comma.hpp"

#include <assert.h>
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

#define _POSIX_C_SOURCE 200809L
#include <locale.h>

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
static PatternList includeDirPatList;
static PatternList excludeDirPatList;
static PickPatList pickPatList;
static StringList fileDirList;

static lstring tableType = "count";
static bool isTable = false;        // -table=count|size|hardlink

static bool showFile = true;
static bool verbose = false;
static unsigned maxDepth = 0;
static bool summary = false;
static bool total = false;
static bool dryrun = false;
static bool divByHardlink = false;
static bool progress = false;

static uint optionErrCnt = 0;
static uint patternErrCnt = 0;

struct DuInfo {
    std::string ext;
    size_t count;
    size_t diskSize;
    size_t fileSize;
    size_t hardlinks;
    DuInfo() : count(0), diskSize(0), fileSize(0), hardlinks(0) {}
};

typedef std::map<std::string, DuInfo> DuList;
DuList duList;

typedef int (*SortByFunc)(const DuInfo& lhs, const DuInfo& rhs);
struct SortBy {
    SortBy(SortBy* _nextSort, SortByFunc _sortFunc, bool _forward) :
        nextSort(_nextSort), sortFunc(_sortFunc), forward(_forward)
    {}
    SortBy* nextSort;
    SortByFunc sortFunc;
    bool forward;
    bool operator()(DuInfo& lhs, DuInfo& rhs) {
        int cmp = sortFunc(lhs, rhs);
        return (cmp == 0) ? ((nextSort == nullptr) ? false : (*nextSort)(lhs, rhs)) : (forward == cmp < 0);
    }
};
int SortByExt(const DuInfo& lhs, const DuInfo& rhs)  { return (lhs.ext.compare(rhs.ext));}
int SortByCount (const DuInfo& lhs, const DuInfo& rhs)  { return (int)(lhs.count - rhs.count);}
int SortByDiskSize(const DuInfo& lhs, const DuInfo& rhs)   { return (int)(lhs.diskSize - rhs.diskSize);}
int SortByFileSize(const DuInfo& lhs, const DuInfo& rhs)   { return (int)(lhs.fileSize - rhs.fileSize);}
SortBy* sortBy = new SortBy(nullptr, SortByExt, true);

std::string separator = "\t";
std::string format = "%8.8e\t%8c\t%15s\n";        // %s\t%8d\t%15d\n";
std::string header = "     Ext\t   Count\t      Size\n";
std::string tformat = format;
std::string sformat = "%S %n\n";
int setBothFmt = 0;
time_t startT, prevT;

// Forward declaration
void clearUsage();
void printUsage(const std::string& filepath);
void buildTable(const std::string& filepath);
void printTable();


//-------------------------------------------------------------------------------------------------
// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
const std::string currentDateTime(time_t& now) {
    now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
    return buf;
}

//-------------------------------------------------------------------------------------------------
// Return true if inName matches pattern in patternList
static
bool FileMatches(const lstring& inName, const PatternList& patternList, bool emptyResult) {
    if (patternList.empty() || inName.empty())
        return emptyResult;

    for (size_t idx = 0; idx != patternList.size(); idx++)
        if (std::regex_match(inName.begin(), inName.end(), patternList[idx]))
            return true;

    return false;
}

//-------------------------------------------------------------------------------------------------
// Open, read and parse file.
static
bool ExamineFile(const lstring& filepath, const lstring& filename) {
    struct stat filestat;
    if (stat(filepath, &filestat) != 0)
        return false;

    lstring ext;
    if (pickPatList.empty()) {
        Directory_files::getExt(ext, filename);
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
    duInfo.ext = ext;
    duInfo.count++;

#ifdef HAVE_WIN
    size_t diskSize = filestat.st_size;    // filestat.st_size;
#else
    size_t diskSize = filestat.st_blocks * filestat.st_blksize;
#endif

    if (filestat.st_nlink > 1)
        duInfo.hardlinks++;

    if (duInfo.hardlinks > 1 && divByHardlink) {
        duInfo.diskSize += diskSize / duInfo.hardlinks;
        duInfo.fileSize += filestat.st_size / duInfo.hardlinks;
    } else {
        duInfo.diskSize += diskSize;
        duInfo.fileSize += filestat.st_size;
    }

    return false;
}


//-------------------------------------------------------------------------------------------------
// Locate matching files which are not in exclude list.
static
size_t FindFile(const lstring& fullname) {
    size_t fileCount = 0;
    lstring name;
    Directory_files::getName(name, fullname);

    if (! name.empty()
        && ! FileMatches(name, excludeFilePatList, false)
        && FileMatches(name, includeFilePatList, true)) {
        if (ExamineFile(fullname, name)) {
            fileCount++;
            if (showFile)
                std::cout << fullname << std::endl;
        }
    }

    return fileCount;
}

//--------------------------------------------------------------------------------------------------
// Recurse over directories, locate files.
static
size_t FindFiles(const lstring& dirname, unsigned depth) {
    Directory_files directory(dirname);
    lstring fullname;

    size_t fileCount = 0;

    struct stat filestat;
    try {
        if (stat(dirname, &filestat) == 0 && S_ISREG(filestat.st_mode)) {
            fileCount += FindFile(dirname);
        }
    } catch (exception ex) {
        // Probably a pattern, let directory scan do its magic.
    }

    while (directory.more()) {
        time_t endT;
        currentDateTime(endT);

        directory.fullName(fullname);
        if (directory.is_directory()) {
            // lstring name;
            // getName(name, fullname);
            if ((maxDepth == 0 || depth < maxDepth)
                    && (! dryrun || depth < 1)
                    && ! FileMatches(fullname, excludeDirPatList, false)
                    && FileMatches(fullname, includeDirPatList, true)) {
                if (verbose) {
                    std::cout << fullname << std::endl;
                } else if (std::difftime(endT, prevT) > 30) {
                    if (progress)
                        std::cerr << std::difftime(endT, startT) << "(sec) " << fullname << "  \r";
                    prevT += 10;
                }
                fileCount += FindFiles(fullname, depth + 1);
            }
        } else if (fullname.length() > 0) {
            fileCount += FindFile(fullname);
        }
    }

    return fileCount;
}

//-------------------------------------------------------------------------------------------------
// Return compiled regular expression from text.
static
std::regex getRegEx(const char* value) {
    try {
        std::string valueStr(value);
        return std::regex(valueStr);
        // return std::regex(valueStr, regex_constants::icase);
    } catch (const std::regex_error& regEx) {
        std::cerr << regEx.what() << ", Pattern=" << value << std::endl;
    }

    patternErrCnt++;
    return std::regex("");
}


//-------------------------------------------------------------------------------------------------
// replace=<fromPat>;<toText>
static
void addPicker(const char* replaceArg) {
    Split parts(replaceArg, ";");
    if (parts.size() == 2) {
        PickPat pickPat;
        pickPat.fromPat = std::regex(parts[0]);
        pickPat.toStr = parts[1];
        pickPatList.push_back(pickPat);
    }
}

//-------------------------------------------------------------------------------------------------
static
void setSortBy(const char* value, bool forward) {
    if (strncasecmp("count", value, strlen(value)) == 0) {
        sortBy = new SortBy(sortBy, SortByCount, forward);
    } else if (strncasecmp("size", value, strlen(value)) == 0) {
        sortBy = new SortBy(sortBy, SortByFileSize, forward);
    } else if (strncasecmp("disk", value, strlen(value)) == 0) {
        sortBy = new SortBy(sortBy, SortByDiskSize, forward);
    } else {
        sortBy = new SortBy(sortBy, SortByExt, forward);
    }
}

//-------------------------------------------------------------------------------------------------
// Convert special characters from text to binary.
static
const char* convertSpecialChar(const char* inPtr) {
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
void printParts(
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

//-------------------------------------------------------------------------------------------------
void showHelp(const char* arg0) {
    const char* helpMsg =
            "  Dennis Lang v2.1 (LandenLabs.com)_X_ " __DATE__ "\n\n"
            "_p_Des: Directory (disk) used space inventory \n"
            "_p_Use: lldu [options] directories...   or  files\n"
            "\n"
            " _p_Options (only first unique characters required, options can be repeated):\n"
            "   -_y_includefile=<filePattern>\n"
            "   -_y_excludefile=<filePattern>\n"
            "   -_y_Includedir=<dirPattern>        ; Match against full dir path \n"
            "   -_y_Excludedir=<dirPattern>        ; Match against full dir path \n"
            "   -_y_verbose\n"
            "   -_y_progress                       ; Show scan progress every 30 sec \n"
            "   -_y_pick=<fromPat>;<toStr>         ; Def: ..*[.](.+);$1 \n"
            "   -_y_format=<format-3-values>       ; Def: %8.8e\\t%8c\\t%15s\\n \n"
            "        e=ext, c=count, l=links, s=size, n=name\n"
            "   -_y_format=<format-3-values>       ; Second format for Total \n"
            "   -_y_FormatSummary=<format-1-value> ; Summary Format, Def: \"%S %e\\n\" \n"
            "   -_y_sort=ext|count|size            ; Def: ext \n"
            "   -_y_reverse=ext|count|size         ; Reverse sort \n"
            "   -_y_header=<header>                ; Def: Ext\\tCount\\tSize\\n \n"
            "   -_y_total                          ; Single report for all inputs \n"
            "   -_y_summary                        ; Single row for each path \n"
            "   -_y_table=count|size|links         ; Present results in table \n"
            "   -_y_divide                         ; Divide size by hardlink count \n"
            "\n"
            " _p_Example:\n"
            "   lldu '-_y_inc=*.bak' -_y_ex=foo.json '-_y_ex=*/subdir2' dir1/subdir dir2 *.txt file2.json \n"
            "   lldu '-_y_exclude=\\.*' '-_y_pick=[^.]+[.](.{4,});other' . \n"
            "   lldu '-_y_exclude=\\.*' '-_y_pick=[^.]+[.](.{4,});other' -_y_sort=size -_y_rev=count . \n"
            "   lldu  -_y_rev=size -_y_rev=count -_y_format='%8e %6c %10s\\n' -_y_for='\\n' -_y_head=' ' . \n"
            "   lldu  -_y_format=\"%9.9e\\t%8c\\t%15s\\n\" -_y_format=\"%9.9e\\t%8c\\t%15s\\n\"  . \n"
            "   lldu  -_y_FormatSummary=\"%8.8n\\t%8c\\t%15s\\n\"  . \n"
            "   lldu  -_y_ver -_y_Include='*/[.][a-zA-Z]*' ~/ \n"
            "\n Show hardlinks (%l or %L format) \n"
            "   lldu  -_y_header=\"   Exten\\tFileSize\\tLinks\\n\" -_y_format=\"%8.8e\\t%8s\\t%5L\\n\"  . \n"
            "\n"
            " _p_Format:\n"
            "    uses standard printf formatting except for these special cases\n"
            "    e=file extension, c=count, s=size, l=links \n"
            "    lowercase c,s,l  format with commas \n"
            "    uppercas  C,S,L  format without commas \n"
            "    preceed with width, ex %12.12e\\t%8c\\t%10s\\n \n"
            "\n"
            " _p_Output:\n"
            "    Ext  Count  Size\n"
            "    ext1 count1 size1 \n"
            "    ext2 count2 size2 \n"
            "    Total count size \n"
            "\n";
    std::cerr << Colors::colorize("\n_W_") << arg0 << Colors::colorize(helpMsg);

    // std::cerr << " Format=" << format << std::endl;
    // std::cerr << " FormatTotal=" << tformat << std::endl;
    // std::cerr << " FormatSummary=" << sformat << std::endl;
}

// ---------------------------------------------------------------------------
void showUnknown(const char* argStr) {
    std::cerr << Colors::colorize("Use -h for help.\n_Y_Unknown option _R_") << argStr << Colors::colorize("_X_\n");
    optionErrCnt++;
}

//-------------------------------------------------------------------------------------------------
// Validate option matchs and optionally report problem to user.
static
bool ValidOption(const char* validCmd, const char* possibleCmd, bool reportErr = true) {
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
bool ValidPattern(PatternList& outList, lstring& value, const char* validCmd, const char* possibleCmd, bool reportErr = true) {
    bool isOk = ValidOption(validCmd, possibleCmd, reportErr);
    if (isOk)  {
        ReplaceAll(value, "*", ".*");
        ReplaceAll(value, "?", ".");
        outList.push_back(getRegEx(value));
        return true;
    }
    return isOk;
}

//-------------------------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc == 1) {
        showHelp(argv[0]);
        return 0;
    } else {
        // Try and setup for %'d printf to output comma. Does not work on MacOS
        setlocale(LC_ALL, "");

        bool doParseCmds = true;
        string endCmds = "--";
        for (int argn = 1; argn < argc; argn++) {
            if (*argv[argn] == '-' && doParseCmds) {
                lstring argStr(argv[argn]);
                Split cmdValue(argStr, "=", 2);
                if (cmdValue.size() == 2) {
                    lstring cmd = cmdValue[0];
                    lstring value = cmdValue[1];
                    const char* cmdName = cmd+1;
                    
                    switch (*cmdName) {
                        case 'd': // depth=0..n
                            if (ValidOption("depth", cmdName))  {
                                maxDepth = atoi(value);
                            }
                            break;
                        case 'e':   // excludeFile=<pat>
                            ValidPattern(excludeFilePatList, value, "excludeFile", cmdName);
                            break;
                        case 'E':   // ExcludeDir=<pat>
                            ValidPattern(excludeDirPatList, value, "ExcludeDir", cmdName);
                            break;
                        case 'f':   // format=<str>
                            if (ValidOption("format", cmdName)) {
                                if (setBothFmt++ == 0)
                                    tformat = format = convertSpecialChar(value);
                                else
                                    tformat = convertSpecialChar(value);
                            }
                            break;
                        case 'F':
                             if (ValidOption("Formatsummary", cmdName)) {
                                sformat = convertSpecialChar(value);
                            }
                            break;
                        case 'h':   // header=<str>
                            if (ValidOption("header", cmdName)) {
                                header = convertSpecialChar(value);
                            }
                            break;
                        case 'i':   // includeFile=<pat>
                            ValidPattern(includeFilePatList, value, "includeFile", cmdName);
                            break;
                        case 'I':   // IncludeDir=<pat>
                            ValidPattern(includeDirPatList, value, "includeDir", cmdName);
                            break;
                        case 'p': // pick=<fromPat>;<toText>
                            if (ValidOption("pick", cmdName)) {
                                addPicker(convertSpecialChar(value));
                            }
                            break;
                        case 'r':
                            if (ValidOption("reverse", cmdName)) {
                                setSortBy(value, false);
                            }
                            break;
                        case 's':
                            if (ValidOption("separator", cmdName, false)) {
                                separator = convertSpecialChar(value);
                            } else if (ValidOption("sort", cmdName)) {
                                setSortBy(value, true);
                            }
                            break;
                        case 't':   // table=count|size|hardlinks
                            if (ValidOption("table", cmdName)) {
                                tableType = value;
                                isTable = true;
                            }
                            break;
                        default:
                            showUnknown(argStr);
                            break;
                    }
                } else {
                    const char* cmdName = argStr + 1;
                    switch (argStr[1]) {
                    case 'd':
                        if (ValidOption("divide", cmdName)) {
                            divByHardlink = true;
                        }
                        break;
                    case 'h':
                        if (ValidOption("help", cmdName)) {
                            showHelp(argv[0]);
                            return 0;
                        }
                        break;
                    case 'n':   // -n (dry run)
                        dryrun = true;
                        break;
                    case 'p':   // -progress
                        if (ValidOption("progress", cmdName)) {
                            progress = true;
                        }
                        break;
                    case 's':   // -summary
                        if (ValidOption("summary", cmdName)) {
                            summary = true;
                        }
                        break;;
                    case 't':   // -total
                        if (ValidOption("total", cmdName)) {
                            total = true;
                        }
                        break;
                    case 'v':   // -v=true or -v=anyThing
                        if (ValidOption("verbose", cmdName)) {
                            verbose = true;
                        }
                        break;
                    case '?':
                        showHelp(argv[0]);
                        return 0;
                    default:
                        showUnknown(argStr);
                    }

                    if (endCmds == argv[argn]) {
                        doParseCmds = false;
                    }
                }
            } else {
                // Store file directories
                fileDirList.push_back(argv[argn]);
            }
        }

        if (pickPatList.empty()) {
            addPicker("..*[.](.+);$1");
        }

        if (patternErrCnt == 0 && optionErrCnt == 0 && fileDirList.size() != 0) {
            if (! summary)
                std::cerr <<  Colors::colorize("_G_ +Start ") << currentDateTime(startT) << Colors::colorize("_X_\n");
            prevT = startT;

            if (fileDirList.size() == 1 && fileDirList[0] == "-") {
                string filePath;
                while (std::getline(std::cin, filePath)) {
                    FindFiles(filePath, 0);
                }
            } else {
                for (auto const& filePath : fileDirList) {
                    FindFiles(filePath, 0);
                    if (isTable) {
                        buildTable(filePath);
                    } else { /* if (!total) */
                        printUsage(filePath);
                    }
                    clearUsage();
                }
            }

            if (isTable) {
                printTable();
            } else {
                printUsage(""); // print grand total
            }
            
            if (! summary) {
                time_t endT;
                currentDateTime(endT);
                std::cerr <<  Colors::colorize("_G_ +End ")
                    << currentDateTime(endT)
                    << ", Elapsed "
                    << std::difftime(endT, startT)
                    << Colors::colorize(" (sec)_X_\n");
            }
        } else {
            // showHelp(argv[0]);
            std::cerr << std::endl;
            return -1;
        }
    }
    
    return 0;
}

//-------------------------------------------------------------------------------------------------
void clearUsage() {
    duList.clear();
}

//-------------------------------------------------------------------------------------------------
size_t gtotalCount = 0;
size_t gtotalLinks = 0;
size_t gtotalDiskSize = 0;
size_t gtotalFileSize = 0;
void printUsage(const std::string& filepath) {
    size_t totalCount = 0;
    size_t totalLinks = 0;
    size_t totalDiskSize = 0;
    size_t totalFileSize = 0;

    if (! summary) {
        printf("\n%s\n", filepath.c_str());
        if (! total)
            printf(header.c_str());
    }

    typedef std::vector<DuInfo> VecDuList;
    VecDuList::const_iterator iter;
    VecDuList vecDuList;
    for (const auto &info : duList)
        vecDuList.push_back(info.second);

    std::sort(vecDuList.begin(), vecDuList.end(), *sortBy);
    for (auto iter = vecDuList.cbegin(); iter != vecDuList.cend(); iter++) {
        if (! summary && ! total) {
            if (format.length() > 0) {
                printParts(format.c_str(), iter->ext.c_str(), iter->count, iter->hardlinks, iter->diskSize);
            } else {
                // std::cout << iter->first << separator << iter->second.count << separator << iter->second.diskSize << std::endl;
            }
        }
        totalCount += iter->count;
        totalLinks += iter->hardlinks;
        totalDiskSize += iter->diskSize;
        totalFileSize += iter->fileSize;
    }

    gtotalCount += totalCount;
    gtotalLinks += totalLinks;
    gtotalDiskSize += totalDiskSize;
    gtotalFileSize += totalFileSize;

    if (summary) {
        if (filepath.empty()) {
            printParts(sformat.c_str(), "_GTotal", gtotalCount, gtotalLinks, gtotalFileSize);
        } else {
            printParts(sformat.c_str(), filepath.c_str(), totalCount, totalLinks, totalFileSize);
        }
    } else {
        if (tformat.length() > 0) {
            if (filepath.empty())
                printParts(tformat.c_str(), "_GTotal", gtotalCount, gtotalLinks, gtotalFileSize);
            else
                printParts(tformat.c_str(), "_Total", totalCount, totalLinks, totalFileSize);
        } else {
            // std::cout << iter->first << separator << iter->second.count << separator << iter->second.diskSize << std::endl;
        }
    }
}

//-------------------------------------------------------------------------------------------------
template <class TT> void appendAt(size_t pos, std::vector<TT>& list, TT data, TT filler) {
    while (list.size() < pos)
        list.push_back(filler);
    list.push_back(data);
}

//-------------------------------------------------------------------------------------------------
typedef std::map<std::string, std::vector<DuInfo>> DuTable;  // ext, array of DuInfo
DuTable tableList;
StringList filePaths;
DuInfo emptyDu;

void buildTable(const std::string& filepath) {
    // Merge DuList into a multi-column table
    size_t column = filePaths.size();
    filePaths.push_back(filepath);
    for (const auto & duItem : duList) {
        appendAt(column, tableList[duItem.second.ext], duItem.second, emptyDu);
    }
}

void printTable() {
    printf("Table of %s\n", tableType.c_str());
    size_t* totals = new size_t[filePaths.size()];
    
    // Print merged table
    for (const auto & duItem : tableList) {
        printf("%10.10s  ", duItem.first.c_str());
        const std::vector<DuInfo>& duList = duItem.second;
        unsigned col = 0;
        for (auto iter = duList.cbegin(); iter != duList.cend(); iter++) {
            size_t value;
            switch (tableType[0]) {
                default:
                case 'c': value = iter->count; break;
                case 'd': value = iter->diskSize; break;
                case 'f': // filesize
                case 's': value = iter->fileSize; break;
                case 'l': // links
                case 'h': value = iter->hardlinks; break;
            }
            printf("%10lu", (unsigned long)value);
            totals[col++] +=  value;
        }
       
        printf("\n");
    }
    
    printf("%10.10s  ", "_TOTAL");
    for (unsigned col = 0; col < filePaths.size(); col++) {
        printf("%10lu", (unsigned long)totals[col]);
    }
    std::cout << "\nPaths:\n";
    for (auto item : filePaths) {
        std::cout << item << std::endl;
    }
}
