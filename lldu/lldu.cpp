//-------------------------------------------------------------------------------------------------
//
//  lldu      Dec-2024       Dennis Lang
//
//  Directory (disk) used space
//
//-------------------------------------------------------------------------------------------------
//
// Author: Dennis Lang - 2024
// https://landenlabs.com/
//
//  
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
#include "signals.hpp"
#include "parseutil.hpp"
#include "directory.hpp"
#include "storage.hpp"

#include <assert.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <regex>
#include <exception>

#define _POSIX_C_SOURCE 200809L

#define VERSION "v6.02" // 2026-Feb

#ifdef HAVE_WIN
#include <direct.h> // _getcwd
#define chdir _chdir
#define getcwd _getcwd
#define lstat stat
#define S_ISLNK(a) false
#else
const size_t MAX_PATH = __DARWIN_MAXPATHLEN;
#endif

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
static PatternList summaryDirPatList;
static PickPatList pickPatList;
static StringList fileDirList;

static lstring tableType = "count";
static bool isTable = false;        // -table=count|size|hardlink

static lstring isSideBySide;   // -colum=size|hardlink|access|modify|create
static std::set<std::string> fileNameList;

static bool showFile = false;
static bool verbose = false;
static unsigned maxDepth = 0;
static bool summary = false;
static bool showAbsPath = false;    // -absolute = show absolute path. 
static bool total = false;
static bool dryrun = false;
static bool divByHardlink = false;
static bool progress = false;
static bool listDev = false;
static size_t progressLen = 0;

const size_t MAX_DIR_DEPTH = 200;

struct DuInfo {
    std::string ext;
    size_t count;
    size_t diskSize;
    size_t fileSize;
    size_t hardlinks;
    size_t softlinks;
    DuInfo() : count(0), diskSize(0), fileSize(0), hardlinks(0), softlinks(0) {}
    DuInfo(std::string _str, size_t _count, size_t _diskSize, size_t _fileSize, size_t _links ) :
        ext(_str), count(_count), diskSize(_diskSize), fileSize(_fileSize), hardlinks(_links) {}
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
SortBy* defSortBy = new SortBy(nullptr, SortByExt, true);
SortBy* sortBy = nullptr;

std::string separator = "\t";
std::string formatDef = "%8.8e\t%8c\t%15s\n";        // %s\t%8d\t%15d\n";
std::string header = "     Ext\t   Count\t      Size\n";
std::string tformat = formatDef;
// std::string sformat = "%10S %n\n";
std::string sformat = "%15s Files:%5c \t HardLinks:%3l\t%n \n";
std::string cformat = "%15.15s\t";  // 1st column name format

int setBothFmt = 0;
time_t startT, prevT;

// Forward declaration
void printTime(time_t epoch, const char* fmtTm);
void clearUsage();
void printUsage(const std::string& filepath);
void buildTable(const std::string& filepath);
void printTable();

static char CWD_BUF[MAX_PATH];
static unsigned CWD_LEN = 0;

//-------------------------------------------------------------------------------------------------
void clearProgress() {
    if (progressLen > 0)
        printf("%*s\r", (int)progressLen, "");
    progressLen = 0;
}

//-------------------------------------------------------------------------------------------------
// Open, read and parse file.
static
bool ExamineFile(const lstring& filepath, const lstring& filename) {
    struct stat filestat;
    // Use lstat to avoid following the link to its target
    if (lstat(filepath, &filestat) != 0)
        return false;

    lstring ext;
    if (pickPatList.empty()) {
        DirUtil::getExt(ext, filename);
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
    if (S_ISLNK(filestat.st_mode))
        duInfo.softlinks++;
    else {
        if (duInfo.hardlinks > 1 && divByHardlink) {
            duInfo.diskSize += diskSize / duInfo.hardlinks;
            duInfo.fileSize += filestat.st_size / duInfo.hardlinks;
        } else {
            duInfo.diskSize += diskSize;
            duInfo.fileSize += filestat.st_size;
        }
    }
    
    if (verbose) {
        std::cout << "File:" << filepath << " DiskSize:" << diskSize << " FileSize:" << filestat.st_size << " HardLinks:" << filestat.st_nlink << std::endl;
    }
    return true;
}


//-------------------------------------------------------------------------------------------------
// Locate matching files which are not in exclude list.
static
size_t FindFile(const lstring& fullname, unsigned depth) {
    size_t fileCount = 0;
    lstring name;
    DirUtil::getName(name, fullname);

    if (! name.empty()
            && !ParseUtil::FileMatches(fullname, excludeDirPatList, false)
            && ParseUtil::FileMatches(fullname, includeDirPatList, true)
            && !ParseUtil::FileMatches(name, excludeFilePatList, false)
            && ParseUtil::FileMatches(name, includeFilePatList, true)) {
        if (ExamineFile(fullname, name)) {
            fileCount++;    // includes soft links (size is ignored for soft links)
            if (showFile)
                std::cout << fullname << std::endl;
        } else {
            int e = errno;
            if (e == EINVAL) {
                cerr << "Invalid " << fullname << std::endl;
            } else if (e == ENOENT) {
                cerr << "Special characters or no permission to: " << fullname << std::endl;
            }
        }

        if (isSideBySide) {
            if (depth == 0)
                fileNameList.insert(name);
            else {
                size_t off = fullname.length(); 
                int dirParts = depth + 1;
                while (dirParts-- > 0 && off != string::npos) {
                    off = fullname.rfind(Directory_files::SLASH_CHAR, off-1);
                }
                if (off == string::npos) 
                    fileNameList.insert(name);
                else {
                    fileNameList.insert(fullname.substr(off+1));
                }
            }
        }
    }

    return fileCount;
}

//-------------------------------------------------------------------------------------------------
// Recurse over directories, locate files.
static
size_t FindFiles(const lstring& dirname, unsigned depth) {
    Directory_files directory(dirname);
    lstring fullname;
    lstring timeStr;
    size_t fileCount = 0;

    struct stat filestat;
    try {
        if (stat(dirname, &filestat) == 0 && S_ISREG(filestat.st_mode)) {
            fileCount += FindFile(dirname, depth);
        }
    } catch (exception ex) {
        // Probably a pattern, let directory scan do its magic.
    }

    bool showTotals = summary && (depth == 0) && (dirname.find('*') != string::npos);

    while (!Signals::aborted && directory.more()) {
        time_t endT;
        ParseUtil::fmtDateTime(timeStr, endT);

        directory.fullName(fullname);
        if (directory.is_directory()) {
            lstring name;
            DirUtil::getName(name, fullname);

            if (isSideBySide)
                fileNameList.insert(name);

            if ((maxDepth == 0 || depth+1 < maxDepth)
                    && (!dryrun || depth < 1)
                    && !ParseUtil::FileMatches(fullname, excludeDirPatList, false)
                //    && ParseUtil::FileMatches(fullname, includeDirPatList, true) 
                    && !ParseUtil::FileMatches(name, excludeFilePatList, false)
                //    && ParseUtil::FileMatches(name, includeFilePatList, true) 
            ) {
                if (verbose) {
                    std::cout << "Dir:" << fullname << std::endl;
                }
                else if (std::difftime(endT, prevT) > 10) {
                    if (progress) {
                        clearProgress();
                        progressLen = 6 + 7 + fullname.length();
                        std::cerr << (size_t)std::difftime(endT, startT) << "(sec) " << fullname << "  \r";
                    }
                    prevT = endT;
                }

                if (fullname.find_first_of('?') == string::npos) { 
                    if (summary && ParseUtil::FileMatches(fullname, summaryDirPatList, false)) {
                        clearUsage();
                    }
                    if (depth < MAX_DIR_DEPTH) {
                        fileCount += FindFiles(fullname, depth + 1);
                    }
                    else {
                        std::cerr << "Exceeded max directory depth " << MAX_DIR_DEPTH << std::endl;
                        std::cerr << fullname << std::endl;
                    }
                }
                else {
                    // '?' not valid part of file name path. 
                    std::cerr << "Invalid file name:" <<fullname << std::endl;
                }

                if (showTotals || ParseUtil::FileMatches(fullname, summaryDirPatList, false)) {
                    if (! isSideBySide) {
                        if (isTable) {
                            buildTable(fullname);
                        } else { /* if (!total) */
                            printUsage(fullname);
                        }
                    }
                    clearUsage();
                }
            }
        } else if (fullname.length() > 0) {
            fileCount += FindFile(fullname, depth);
        }
    }

    return fileCount;
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
void showHelp(const char* arg0) {
    const char* helpMsg =
            "  Dennis Lang " VERSION " (LandenLabs.com)_X_ " __DATE__ "\n\n"
            "_p_Des: Directory (disk) used space inventory \n"
            "_p_Use: lldu [options] directories...   or  files\n"
            "\n"
            " _p_Options (only first unique characters required, options can be repeated):\n"
            "   -_y_includeItem=<fileOrDirPattern>\n"
            "   -_y_excludeItem=<fileOrDirPattern> ; Exclude file or directory item\n"
            "   Note - following start with uppercase I or E \n"
            "   -_y_IncludePath=<pathPattern>      ; Match against full dir path \n"
            "   -_y_ExcludePath=<pathPattern>      ; Match against full dir path \n"
            "   NOTE - Patterns above - remember to escape backslash as \\\\ \n"
            "   -_y_verbose\n"
            "   -_y_progress                       ; Show scan progress every 30 sec \n"
            "   -_y_pick=<fromPat>;<toStr>         ; Def: ..*[.](.+);$1 \n"
            "   -_y_format=<format-3-values>       ; Def: %8.8e\\t%8c\\t%15s\\n \n"
            "        e=ext, c=count, l=links, s=size, n=name\n"
            "   -_y_format=<format-3-values>       ; Second format for Total \n"
            "   -_y_FormatSummary=<format-1-value> ; Summary Format, Def: \"%15s Files:%5c \\t%n\" \n"
            "   -_y_sort=ext|count|size            ; Def: ext \n"
            "   -_y_reverse=ext|count|size         ; Reverse sort \n"
            "   -_y_header=<header>                ; Def: Ext\\tCount\\tSize\\n \n"
            "   -_y_total                          ; Single report for all inputs \n"
            "   -_y_summary                        ; Single row for each path \n"
            "   -_y_summary=<dirPat>               ; Sumarize matching dirs \n"
            "   -_y_table=count|size|links         ; Present results in table \n"
            "   -_y_divide                         ; Divide size by hardlink count \n"
            "\n"
            "   -_y_column=access|create|modify|size|link ; Side-by-size 2 or more dirs\n"
            "   -_y_CFMT=%15.15s\\t               ; 1st col format name\n"
            "\n"
            "   -_y_regex                       ; Use regex pattern not DOS pattern \n"
            "   NOTE - Default DOS pattern internally treats * at .*, . at [.] and ? at . \n "
            "          If using -_y_regex specify before pattern options\n"
            "          Use -_y_regex if you need advance pattern syntax \n"
            "   Example to ignore all dot directories and files: \n"
            "          -_y_regex -_y_exclude=\"[.].*\" \n"
            "        or with DOS pattern\n"
            "          -_y_exclude=\".*\" \n"
            "\n"
            " _p_Special Commands:\n"
            "    -_y_list                          ; List devices & storage size "
            "\n\n"
            " _p_Example:\n"
            "   lldu  -_y_sum -_y_Exc=*.git  * \n"
#ifdef HAVE_WIN
            "   lldu  -_y_sum -_y_Exc=*\\\\.git  * \n"
            "   lldu  -_y_sum -_y_Exc=*\\\\.(git|vs) * \n"
            "   lldu  -_y_sum -_y_exe=.git -_y_exe=.vs  * \n"
            "   lldu  -_y_sum -_y_regex -_y_Exc=.*\\\\[.](git|vs) * \n"
            "   lldu  -_y_regex -_y_sum=.*\\\\[.](git|vs) * \n"
#else
            "   lldu  -_y_sum -_y_Exc='*/.git'  * \n"
            "   lldu  -_y_sum -_y_Exc='*/.(git|vs)' * \n"
            "   lldu  -_y_sum -_y_regex -_y_Exc='.*/[.](git|vs)' * \n"
            "   lldu  -_y_regex -_y_sum='*/[.](git|vs)' * \n" 
#endif
            "   lldu  -_y_sum -_y_regex -_y_exc=\"[.](git||vs)\" * \n"
            "   lldu  -_y_formatSum=\"%15s Files:%5c Links:%l\\t %n\\n\" -sum ..\\*"
            "\n"
            "   lldu '-_y_inc=*.bak' -_y_ex=foo.json '-_y_ex=*/subdir2' dir1/subdir dir2 *.txt file2.json \n"
            "   lldu '-_y_exclude=\\.*' '-_y_pick=[^.]+[.](.{4,});other' . \n"
            "   lldu '-_y_exclude=\\.*' '-_y_pick=[^.]+[.](.{4,});other' -_y_sort=size -_y_rev=count . \n"
            "   lldu  -_y_rev=size -_y_rev=count -_y_format='%8e %6c %20s\\n' -_y_for='\\n' -_y_head=' ' . \n"
            "   lldu  -_y_format=\"%9.9e\\t%8c\\t%15s\\n\" -_y_format=\"%9.9e\\t%8c\\t%15s\\n\"  . \n"
            "   lldu  -_y_FormatSummary=\"%8.8n\\t%8c\\t%15s\\n\"  . \n"
            "   lldu  -_y_ver -_y_Include='*/[.][a-zA-Z]*' ~/ \n"
            "\n Show hardlinks (%l or %L format) \n"
            "   lldu  -_y_header=\"   Exten\\tFileSize\\tLinks\\n\" -_y_format=\"%8.8e\\t%8s\\t%5L\\n\"  . \n"
            "\n Side-by-side \n"
            "   lldu  -_y_CFMT=\" % 25.25s\\t\"  -_y_col=size -d=2 dir1 dir2 \n"
            "\n"
            " _p_Format:\n"
            "    uses standard printf formatting except for these special cases\n"
            "    e=file extension, c=count, s=size, l=links, n=name (with summary)\n"
            "    lowercase c,s,l  format with commas \n"
            "    uppercase  C,S,L  format without commas \n"
            "    precede with width, ex %12.12e\\t%8c\\t%15s\\n \n"
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

//-------------------------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    Signals::init();
    ParseUtil parser;
    lstring timeStr;

    if (argc == 1) {
        showHelp(argv[0]);
        return 0;
    } else {
        // Try and setup for %'d printf to output comma. Does not work on MacOS
        setlocale(LC_ALL, "");

        getcwd(CWD_BUF, sizeof(CWD_BUF));
        CWD_LEN = (unsigned)strlen(CWD_BUF);
        CWD_BUF[CWD_LEN++] = Directory_files::SLASH_CHAR;
        CWD_BUF[CWD_LEN] = '\0';

        bool doParseCmds = true;
        string endCmds = "--";
        for (int argn = 1; argn < argc; argn++) {
            if (*argv[argn] == '-' && doParseCmds) {
                lstring argStr(argv[argn]);
                Split cmdValue(argStr, "=", 2);
                if (cmdValue.size() == 2) {
                    lstring cmd = cmdValue[0];
                    lstring value = cmdValue[1];

                    const char* cmdName = cmd + 1;
                    if (cmd.length() > 2 && *cmdName == '-')
                        cmdName++;  // allow -- prefix on commands
                    switch (*cmdName) {
                        case 'c':   // column=count|size|hardlinks|file
                            if (parser.validOption("colum", cmdName)) {
                                isSideBySide = value;
                                maxDepth = 1;
                            }
                            break;
                        case 'C':  // CFMT cformat
                            if (parser.validOption("cfmt", cmdName)) {
                                cformat = ParseUtil::convertSpecialChar(value);
                            }
                            break;
                        case 'd': // depth=0..n
                            if (parser.validOption("depth", cmdName))  {
                                maxDepth = atoi(value);
                            }
                            break;
                        case 'e':   // excludeItem=<patFile>
                            parser.validPattern(excludeFilePatList, value, "excludeItem", cmdName);
                            break;
                        case 'E':   // ExcludePath=<patFile>
                            parser.validPattern(excludeDirPatList, value, "ExcludePath", cmdName);
                            break;
                        case 'f':   // format=<str>
                            if (parser.validOption("format", cmdName, false)) {
                                if (setBothFmt++ == 0)
                                    tformat = formatDef = ParseUtil::convertSpecialChar(value);
                                else
                                    tformat = ParseUtil::convertSpecialChar(value);
                            } else if (parser.validOption("formatSummary", cmdName)) {
                                sformat = ParseUtil::convertSpecialChar(value);
                            }
                            break;
                        case 'F':
                            if (parser.validOption("formatSummary", cmdName)) {
                                sformat = ParseUtil::convertSpecialChar(value);
                            }
                            break;
                        case 'h':   // header=<str>
                            if (parser.validOption("header", cmdName)) {
                                header = ParseUtil::convertSpecialChar(value);
                            }
                            break;
                        case 'i':   // includeItem=<patFile>
                            parser.validPattern(includeFilePatList, value, "includeItem", cmdName);
                            break;
                        case 'I':   // IncludePath=<patFile>
                            parser.validPattern(includeDirPatList, value, "IncludePath", cmdName);
                            break;
                        case 'p': // pick=<fromPat>;<toText>
                            if (parser.validOption("pick", cmdName)) {
                                addPicker(ParseUtil::convertSpecialChar(value));
                            }
                            break;
                        case 'r':
                            if (parser.validOption("reverse", cmdName)) {
                                setSortBy(value, false);
                            }
                            break;
                        case 's':
                            if (parser.validOption("separator", cmdName, false)) {
                                separator = ParseUtil::convertSpecialChar(value);
                            } else if (parser.validOption("sort", cmdName, false)) {
                                setSortBy(value, true);
                            } else if (parser.validPattern(summaryDirPatList, value, "summary", cmdName, false)) {
                                summary = true;
#ifdef HAVE_WIN
                                lstring incDirPat = value + Directory_files::SLASH2 + ".*";
#else
                                lstring incDirPat = value + Directory_files::SLASH + ".*";
#endif
                                std::regex pat = parser.ignoreCase ? std::regex(incDirPat, regex_constants::icase) : std::regex(incDirPat);
                                includeDirPatList.push_back(pat);
                            } 
                            break;
                        case 't':   // table=count|size|hardlinks|file
                            if (parser.validOption("table", cmdName)) {
                                tableType = value;
                                isTable = true;
                            }
                            break;
                        default:
                            parser.showUnknown(argStr);
                            break;
                    }
                } else {
                    const char* cmdName = argStr + 1;
                    if (argStr.length() > 2 && *cmdName == '-')
                        cmdName++;  // allow -- prefix on commands
                    switch (*cmdName) {
                    case 'a':
                        showAbsPath = parser.validOption("absolute", cmdName);
                        break;
                    case 'd':
                        divByHardlink = parser.validOption("divide", cmdName);
                        break;
                    case 'h':
                        if (parser.validOption("help", cmdName)) {
                            showHelp(argv[0]);
                            return 0;
                        }
                        break;
                    case 'l':   // -list
                        listDev = parser.validOption("list", cmdName);
                        break;
                    case 'n':   // -n (dry run)
                        dryrun = true;
                        break;
                    case 'p':   // -progress
                        progress = parser.validOption("progress", cmdName);
                        break;
                    case 'r':   // -regex
                        parser.unixRegEx = parser.validOption("regex", cmdName);
                        break;
                    case 's':   // -summary
                        summary = parser.validOption("summary", cmdName);
                        break;
                    case 't':   // -total
                        total = parser.validOption("total", cmdName);
                        break;
                    case 'v':   // -v=true or -v=anyThing
                        verbose = parser.validOption("verbose", cmdName);
                        break;
                    case '?':
                        showHelp(argv[0]);
                        return 0;
                    default:
                        parser.showUnknown(argStr);
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

       
        if (parser.patternErrCnt == 0 && parser.optionErrCnt == 0) {
            if (listDev) {
                Storage::ListStorageSizes();
            } else if (fileDirList.size() != 0) {
                ParseUtil::fmtDateTime(timeStr, startT);
                prevT = startT;
                if (! summary)
                    std::cerr << Colors::colorize("_G_ +Start ") << timeStr << Colors::colorize("_X_\n");

                if (fileDirList.size() == 1 && fileDirList[0] == "-") {
                    string filePath;
                    while (std::getline(std::cin, filePath)) {
                        FindFiles(filePath, 0);
                    }
                } else {
                    for (auto const& filePath : fileDirList) {
                        FindFiles(filePath, 0); 
                        if (! isSideBySide) {
                            if (isTable) {
                                buildTable(filePath);
                            } else { /* if (!total) */
                                printUsage(filePath);
                            }
                        }
                        clearUsage();
                    }
                    
                }

                if ( ! isSideBySide.empty()) {
                    struct stat filestat;
                    // std::sort(fileNameList.begin(), fileNameList.end());
                    const char* cfmt = cformat.c_str();
                    printf(cfmt, "Name");
                    for (auto const& filePath : fileDirList) {
                        int len = filePath.length();
                        printf("%15.15s\t", filePath.c_str() + std::max(0, len-15));
                    }
                    printf("\n");
                    for (auto const& name : fileNameList) {

                        if (Signals::aborted)
                            break;

                        printf(cfmt, name.c_str());
                        for (auto const& filePath : fileDirList) {
                            string fullname = filePath + Directory_files::SLASH + name;
                            // Use lstat to avoid following the link to its target
                            if (lstat(fullname.c_str(), &filestat) == 0) {  
                                // printf("%15d ", filestat.st_size);
                                time_t tvalue;
                                size_t value;
                                switch (isSideBySide[0]) {
                                default:value = filestat.st_size; break;
                                case 'a': tvalue = filestat.st_atime; break; // access
                                case 'c': tvalue = filestat.st_ctime; break; // create
                                case 'm': tvalue = filestat.st_mtime; break; // modify
                                case 's': value = filestat.st_size; break;
                                case 'l': // links
                                case 'h': value = filestat.st_nlink; break;
                                }

                                switch (isSideBySide[0]) {
                                case 'a': 
                                case 'c': 
                                case 'm': 
                                    printTime(tvalue, "%d-%b-%y %H:%M\t");
                                    break;
                                default:
                                    printf("%15lu ", (unsigned long)value);
                                }
                            } else {
                                printf("%15.15s\t", "--");
                            }
                        }
                        printf("\n");
                    }
                } if (isTable) {
                    printTable();
                } else {
                    printUsage(""); // print grand total
                }

                if (! summary) {
                    time_t endT;
                    ParseUtil::fmtDateTime(timeStr, endT);
                    std::cerr << Colors::colorize("_G_ +End ")
                        << timeStr
                        << ", Elapsed "
                        << std::difftime(endT, startT)
                        << Colors::colorize(" (sec)_X_\n");
                }
            }
        } else {
            // showHelp(argv[0]);
            std::cerr << std::endl;
            return -1;
        }
    }
    
    return 0;
}


void printTime(time_t epoch, const char* tmFmt) {
    struct tm* localTm;
    localTm = localtime(&epoch);
    char timbuf[80];
    strftime(timbuf, sizeof(timbuf), tmFmt, localTm);
    printf(timbuf);
}

//-------------------------------------------------------------------------------------------------
void clearUsage() {
    // std::cerr << "Clear usage\n";
    duList.clear();
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
    const char* FMT_NUMB = "zu";
    const char* FMT_COMMA = "zu";
#else
    const char* FMT_COMMA = "'lu";  //  "`lu" linux supports ` to add commas
    const char* FMT_NUMB = "lu";     
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
                itemFmt += FMT_NUMB;        // unsigned long formatter
                printf(itemFmt, count);     // print with commas
                break;
            case 'c':   // Count
                itemFmt += FMT_COMMA;       // unsigned long formatter
                PRINTF(itemFmt, count);     // print with commas
                break;
            case 'L':   // Links
                // DisableCommaCout();
                itemFmt += FMT_NUMB;        // unsigned long formatter
                printf(itemFmt, links);
                break;
            case 'l':   // Links
                itemFmt += FMT_COMMA;       // unsigned long formatter
                PRINTF(itemFmt, links);
                break;
            case 'S':   // Size
                // DisableCommaCout();
                itemFmt += FMT_NUMB;        // unsigned long formatter
                printf(itemFmt, size);
                break;
            case 's':   // Size
                itemFmt += FMT_COMMA;       // unsigned long formatter
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
size_t gtotalCount = 0;
size_t gtotalLinks = 0;
size_t gtotalDiskSize = 0;
size_t gtotalFileSize = 0;
std::vector<DuInfo> summaryInfos;

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

    if (sortBy == nullptr)
        sortBy = defSortBy;
    
    std::sort(vecDuList.begin(), vecDuList.end(), *sortBy);
    for (auto iter = vecDuList.cbegin(); iter != vecDuList.cend(); iter++) {
        if (! summary && ! total) {
            if (formatDef.length() > 0) {
                printParts(formatDef.c_str(), iter->ext.c_str(), iter->count, iter->hardlinks, iter->diskSize);
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
            if (summaryInfos.size() > 0) {
                std::sort(summaryInfos.begin(), summaryInfos.end(), *sortBy);
                for (auto iter = summaryInfos.cbegin(); iter != summaryInfos.cend(); iter++) {
                    string sumPath = iter->ext;
                    unsigned off = 0;
                    if (!showAbsPath && sumPath.length() > CWD_LEN+1 && strncmp(sumPath.c_str(), CWD_BUF, CWD_LEN) == 0)
                        off = CWD_LEN;
                    printParts(sformat.c_str(), sumPath.c_str() + off, iter->count, iter->hardlinks, iter->fileSize);
                }
                summaryInfos.clear();
            }
            printParts(sformat.c_str(), "_GTotal", gtotalCount, gtotalLinks, gtotalFileSize);
        } else {
            unsigned off = 0;
            if (!showAbsPath && filepath.length() > CWD_LEN+1 && strncmp(filepath.c_str(), CWD_BUF, CWD_LEN) == 0) 
                off = CWD_LEN;
            
            clearProgress();
            if (sortBy == nullptr) {
                printParts(sformat.c_str(), filepath.c_str() + off, totalCount, totalLinks, totalFileSize);
            } else {
                summaryInfos.push_back(DuInfo(filepath, totalCount, totalDiskSize, totalFileSize, totalLinks));
            }
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
    for (size_t col = 0; col < filePaths.size(); col++)
        totals[col] = 0;

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
            totals[col++] += value;
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
