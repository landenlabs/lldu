//-------------------------------------------------------------------------------------------------
//
//  lldupdir      Feb-2024       Dennis Lang
//
//  Find duplicate files - similar to lldup but faster for parallel dir paths. 
//
//-------------------------------------------------------------------------------------------------
//
// Author: Dennis Lang - 2024
// https://landenlabs.com/
//
// This file is part of lldupdir project.
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
#include "command.hpp"
#include "dupscan.hpp"


#include <fstream>
#include <iostream>
#include <vector>
#include <regex>
#include <exception>

using namespace std;

// Helper types
typedef unsigned int uint;

// ---------------------------------------------------------------------------
// Dive into directories, locate files.
static size_t InspectFiles(Command& command, const lstring& dirname) {
    Directory_files directory(dirname);
    lstring fullname;

    size_t fileCount = 0;

    struct stat filestat;
    try {
        if (stat(dirname, &filestat) == 0 && S_ISREG(filestat.st_mode)) {
            fileCount += command.add(dirname);
            return fileCount;
        }
    } catch (const exception& ex) {
        // Probably a pattern, let directory scan do its magic.
    }

    while (!Signals::aborted &&  directory.more()) {
        directory.fullName(fullname);
        if (directory.is_directory()) {
            fileCount += InspectFiles(command, fullname);
        } else if (fullname.length() > 0) {
            fileCount += command.add(fullname);
        }
    }

    return fileCount;
}

// ---------------------------------------------------------------------------
void showHelp(const char* arg0) {
    const char* helpMsg = "  Dennis Lang v2.5 (landenlabs.com) " __DATE__ "\n\n"
        "_p_Des: 'Find duplicate files by comparing length, hash value and optional name. \n"
        "_p_Use: lldupdir [options] directories...   or  files\n"
        "\n"
        "_p_Options (only first unique characters required, options can be repeated): \n"
        "\n"
        "\n"
        //    "   -invert           ; Invert test output "
        "   -_y_includeFile=<filePattern>   ; -inc=*.java \n"
        "   -_y_excludeFile=<filePattern>   ; -exc=*.bat -exe=*.exe \n"
        "   _p_Note: Capitalized _y_I_x_nclude/_y_E_x_xclude for full path pattern \n"
#ifdef HAVE_WIN
        "   _p_Note: Escape directory slash on windows \n"
        "   -_y_IncludePath=<pathPattern>   ; -Inc=*\\\\code\\\\*.java \n"
        "   -_y_ExcludePath=<pathPattern>   ; -Exc=*\\\\x64\\\\* -Exe=*\\\\build\\\\* \n"
#else
        "   -_y_IncludePath=<pathPattern>   ; -Inc=*/code/*.java \n"
        "   -_y_ExcludePath=<pathPattern>   ; -Exc=*/bin/* -Exe=*/build/* \n"
#endif
        "\n"
        "   -_y_regex                       ; Use regex pattern not DOS pattern \n"
        "   NOTE - Default DOS pattern converts * to .*, . to [.] and ? to . \n "
        "          If using -_y_regex specify before pattern options\n"
        "   Example to ignore all dot directories and files: \n"
        "          -_y_regex -_y_exclude=\"[.].*\" \n"
        "\n"
        "   -verbose \n"
        "   -quiet \n"

        "\n"
        "_p_Options (default show only duplicates):\n"
        "   -_y_showAll            ; Compare all files for matching hash \n"
        "   -_y_showDiff           ; Show files that differ\n"
        "   -_y_showMiss           ; Show missing files \n"
        "   -_y_hideDup            ; Don't show duplicate files  \n"
        "   -_y_showAbs            ; Show absolute file paths  \n"

        "   -_y_preDup=<text>      ; Prefix before duplicates, default nothing  \n"
        "   -_y_preDiff=<text>     ; Prefix before differences, default: \"!= \"  \n"
        "   -_y_preMiss=<text>     ; Prefix before missing, default: \"--  \" \n"
        "   -_y_postDivider=<text> ; Divider for dup and diff, def: \"__\\n\"  \n"
        "   -_y_separator=<text>   ; Separator, def: \", \"  \n"
        "\n"
        "_p_Options (when scanning two directories, dup if names and hash match) :\n"
        "   -_y_simple                      ; Show files no prefix or separators \n"
        "   -_y_log=[first|second]          ; Only show 1st or 2nd file for Dup or Diff \n"
        "   -_y_no                          ; DryRun, show delete but don't do delete \n"
        "   -_y_delete=[first|second|both]  ; If dup or diff, delete 1st, 2nd or both files \n"

        "   -_y_threads                     ; Compute file hashes in threads \n"
        "\n"
        "_p_Options (when comparing one dir or 3 or more directories)\n"
        "        Default compares all files for matching length and hash value\n"
        "   -_y_justName                    ; Match duplicate name only, not contents \n"
        "   -_y_ignoreExtn                  ; With -justName, also ignore extension \n"
        "   -_y_all                         ; Find all matches, ignore name \n"
        "   -_y_delDupPat=pathPat           ; If dup   delete if pattern match \n"

        //        "   -ignoreHardlinks   ; \n"
        //        "   -ignoreSoftlinks    ; \n"
        "\n"
        "_p_Examples: \n"
        "  Find file matches by name and hash value (_P_fastest with only 2 dirs_X_) \n"
        "   lldupdir  dir1 dir2/subdir  \n"
        "   lldupdir  -_y_showMiss -_y_showDiff dir1 dir2/subdir  \n"
        "   lldupdir  -_y_hideDup -_y_showMiss -_y_showDiff dir1 dir2/subdir  \n"
#ifdef HAVE_WIN
        "   lldupdir -_y_Exc=*\\\\.git  -_y_exc=*.exe -_y_exc=*.zip -_y_hideDup -_y_showMiss   dir1 dir2/subdir  \n"
        "   lldupdir -_y_exc=.git -_y_exc=.cs -_y_exc=*.exe -_y_exc=*.zip -_y_hideDup -_y_showMiss   dir1 dir2/subdir  \n"
#else
        "   lldupdir -_y_Exc=\\*/.git -_y_exc=\\*.exe -_y_hideDup -_y_showMiss   dir1 dir2/subdir  \n"
        "   lldupdir -_y_exc=.git -_y_exc=\\*.exe -_y_hideDup -_y_showMiss   dir1 dir2/subdir  \n"
#endif
        "\n"
        "  Find file matches by matching hash value, slower than above, 1 or three or more dirs \n"
        "   lldupdir  -_y_showAll  dir1 \n"
        "   lldupdir  -_y_showAll  dir1   dir2/subdir   dir3 \n"
        "\n"
        "  Change how output appears \n"
        "   lldupdir  -_y_sep=\" /  \"  dir1 dir2/subdir dir3\n"
        "\n"
        "\n";

    std::cerr << Colors::colorize("\n_W_") << arg0 << Colors::colorize(helpMsg);
}

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

// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    Signals::init();
    ParseUtil parser;
    DupFiles dupFiles;
    Command* commandPtr = &dupFiles;
    StringList extraDirList;

    if (argc == 1) {
        showHelp(argv[0]);
    } else {
        bool doParseCmds = true;
        string endCmds = "--";
        for (int argn = 1; argn < argc; argn++) {
            if (*argv[argn] == '-' && doParseCmds) {
                lstring argStr(argv[argn]);
                Split cmdValue(argStr, "=", 2);
                if (cmdValue.size() == 2) {
                    lstring cmd = cmdValue[0];
                    lstring value = cmdValue[1];
                    
                    if (cmd.length() > 1 && cmd[0] == '-')
                        cmd.erase(0);   // allow -- prefix on commands
                    
                    const char* cmdName = cmd + 1;
                    switch (*cmdName) {
                     case 'd':   // delete=None|First|Second|Both
                        if (parser.validOption("deleteFile", cmdName, false)) {
                            if (!Command::getFileTypes(commandPtr->deleteFiles, value)) {
                                parser.showUnknown(argStr);
                                std::cerr << "Valid delete types are: first, second or both\n";
                            }
                        } else 
                            parser.validPattern(commandPtr->delDupPathPatList, value, "delDupPat", cmdName);
                        break;
                    case 'e':   // -excludeFile=<patFile>
                        parser.validPattern(commandPtr->excludeFilePatList, value, "excludeFile", cmdName);
                        break;
                    case 'E':   // -ExcludeDir=<patPath>
                        parser.validPattern(commandPtr->excludePathPatList, value, "ExcludeDir", cmdName);
                        break;
                    case 'i':   // -includeFile=<patFile>
                        parser.validPattern(commandPtr->includeFilePatList, value, "includeFile", cmdName);
                        break;
                    case 'I':   // -IncludeDir=<patPath>
                        parser.validPattern(commandPtr->includePathPatList, value, "includeDir", cmdName);
                        break;
                    case 'l':   // log=First|Second   def=Both
                        if (parser.validOption("log", cmd + 1)) {
                            if (!Command::getFileTypes(commandPtr->showFiles, value)) {
                                parser.showUnknown(argStr);
                                std::cerr << "Valid log types are: first, second or both\n";
                            }
                        }
                        break;
                    case 'p':
                        if (parser.validOption("postDivider", cmdName, false)) {
                            commandPtr->postDivider = ParseUtil::convertSpecialChar(value);
                        } else if (parser.validOption("preDivider", cmdName, false)) {
                            commandPtr->preDivider = ParseUtil::convertSpecialChar(value);
                        } else if (parser.validOption("preDuplicate", cmdName, false)) {
                            commandPtr->preDup = ParseUtil::convertSpecialChar(value);
                        } else if (parser.validOption("preDiffer", cmdName, false)) {
                            commandPtr->preDiff = ParseUtil::convertSpecialChar(value);
                        } else if (parser.validOption("preMissing", cmdName)) {
                            commandPtr->preMissing = ParseUtil::convertSpecialChar(value);
                        }
                        break;
                    case 's':
                        if (parser.validOption("separator", cmdName)) {
                            commandPtr->separator = ParseUtil::convertSpecialChar(value);
                        }
                        break;

                    default:
                        parser.showUnknown(argStr);
                        break;
                    }
                } else {
                    const char* cmdName = argStr + 1;
                    switch (argStr[1]) {
                    case 'a':
                        if (parser.validOption("all", cmdName)) {
                            commandPtr->sameName = false;
                        }
                        break;
                    case 'f': // duplicated files
                        if (parser.validOption("files", cmdName)) {
                            commandPtr = &dupFiles.share(*commandPtr);
                        }
                        break;
                    case '?':
                        showHelp(argv[0]);
                        return 0;
                    case 'h':
                        if (parser.validOption("help", cmdName, false)) {
                            showHelp(argv[0]);
                            return 0;
                        } else if (parser.validOption("hideDup", cmdName)) {
                            commandPtr->showSame = false;
                        }
                        break;
                    case 'i':
                        if (parser.validOption("invert", cmdName, false)) {
                            commandPtr->invert = true;
                        } else if (parser.validOption("ignoreExtn", cmdName)) {
                            commandPtr->ignoreExtn = true;
                        }
                        break;
                    case 'j':
                        if (parser.validOption("justName", cmdName)) {
                            commandPtr->justName = true;
                        }
                        break;
                    case 'n':   // no action, dry-run
                        std::cerr << "DryRun enabled\n";
                        commandPtr->dryRun = true;
                        break;
                    case 'r':   // -regex
                        parser.unixRegEx = parser.validOption("regex", cmdName);
                        break;
                    case 's':
                        if (parser.validOption("showAll", cmdName, false)) {
                            commandPtr->showSame = commandPtr->showDiff = commandPtr->showMiss = true;
                        } else if (parser.validOption("showDiff", cmdName, false)) {
                            commandPtr->showDiff = true;
                        } else if (parser.validOption("showMiss", cmdName, false)) {
                            commandPtr->showMiss = true;
                        } else if (parser.validOption("showSame", cmdName, false)) {
                            commandPtr->showSame = true;        // default
                        } else if (parser.validOption("showAbs", cmdName, false)) {
                            commandPtr->showAbsPath = true;
                        } else if (parser.validOption("sameName", cmdName, false)) {
                            commandPtr->sameName = true;
                        } else if (parser.validOption("simple", cmdName)) {
                            commandPtr->preDup = commandPtr->preDiff = "";
                            commandPtr->separator = " ";
                            commandPtr->postDivider = "\n";
                        }
                        break;
                    case 't':
                        if (parser.validOption("threads", cmdName)) {
                            commandPtr->useThreads = true;
                        }
                        break;
                    case 'q':
                        if (parser.validOption("quiet", cmdName)) {
                            commandPtr->quiet++;
                            commandPtr->showSame = commandPtr->showFile = commandPtr->showDiff = commandPtr->showMiss = false;
                        }
                        break;
                    case 'v':
                        if (parser.validOption("verbose", cmdName)) {
                            commandPtr->verbose = true;
                        }
                        break;
                            
                    default:
                        parser.showUnknown(argStr);
                        break;
                    }

                    if (endCmds == argv[argn]) {
                        doParseCmds = false;
                    }
                }
            }  else {
                // Collect extra arguments for scanning below.
                extraDirList.push_back(argv[argn]);
            }
        }

        unsigned level = 0;
        time_t startT;
        currentDateTime(startT);
        
        if (commandPtr->quiet < 2)
            std::cerr << Colors::colorize("\n_G_ +Start ") << currentDateTime(startT) << Colors::colorize("_X_\n");

        if (commandPtr->begin(extraDirList)) {

            if (parser.patternErrCnt == 0 && parser.optionErrCnt == 0 && extraDirList.size() != 0) {
                if (extraDirList.size() == 1 && extraDirList[0] == "-") {
                    string filePath;
                    while (std::getline(std::cin, filePath)) {
                         size_t fileCnt = InspectFiles(*commandPtr, filePath);
                         if (commandPtr->quiet < 1)
                            std::cerr << "  Files Checked=" << fileCnt << std::endl;
                    }
                } else if (commandPtr->ignoreExtn || !commandPtr->sameName || extraDirList.size() != 2) {
                    for (auto const& filePath : extraDirList) {
                        size_t fileCnt = InspectFiles(*commandPtr, filePath);
                        if (commandPtr->quiet < 1)
                            std::cerr << "  Files Checked=" << fileCnt << std::endl;
                    }
                } else if (extraDirList.size() == 2) {
                    DupScan dupScan(*commandPtr);
                    StringSet nextDirList;
                    nextDirList.insert("");
                  
                    while (!Signals::aborted && dupScan.findDuplicates(level, extraDirList, nextDirList)) {
                        level++;
                    }

                    dupScan.done();
                }
            }

            commandPtr->end();
        }

        if (commandPtr->quiet < 2)
            std::cerr << Colors::colorize("_G_ +Levels=") << level
            << " Dup=" << commandPtr->sameCnt
            << " Diff=" << commandPtr->diffCnt
            << " Miss=" << commandPtr->missCnt
            << " Skip=" << commandPtr->skipCnt
            << " Files=" << commandPtr->sameCnt + commandPtr->diffCnt + commandPtr->missCnt + commandPtr->skipCnt
            << Colors::colorize("_X_\n");

        time_t endT;
        if (commandPtr->quiet < 2) {
            currentDateTime(endT);
            std::cerr << Colors::colorize("_G_ +End ")
                << currentDateTime(endT)
                << ", Elapsed "
                << std::difftime(endT, startT)
                << Colors::colorize(" (sec)_X_\n");
        }
    }

    return 0;
}
