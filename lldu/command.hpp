//-------------------------------------------------------------------------------------------------
// File: Directory.h    Author: Dennis Lang
//
// Desc: This class is used to obtain the names of files in a directory.
//
// Usage::
//      Create a Directory_files object by providing the name of the directory
//      to use.  'next_file_name()' returns the next file name found in the
//      directory, if any.  You MUST check for the existance of more files
//      by using 'more_files()' between each call to "next_file_name()",
//      it tells you if there are more files AND sequences you to the next
//      file in the directory.
//
//      The normal usage will be something like this:
//          Directory_files dirfiles( dirName);
//          while (dirfiles.more_files())
//          {   ...
//              lstring filename = dirfiles.name();
//              ...
//          }
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

#pragma once
#include "ll_stdhdr.hpp"
#include "lstring.hpp"

#include <vector>
#include <regex>

// Helper types
typedef std::vector<lstring> StringList;
typedef std::vector<std::regex> PatternList;
typedef unsigned int uint;
typedef std::vector<unsigned> IntList;

// ---------------------------------------------------------------------------
class Command {
public:
    // Runtime options
    PatternList includeFilePatList;
    PatternList excludeFilePatList;
    PatternList includePathPatList;
    PatternList excludePathPatList;
    PatternList delDupPathPatList;
    // PatternList delDifPathPatList;  // not used

    bool showFile = false;
    bool verbose = false;
    bool invert  = false;
    bool sameName = true;
    bool justName = false;
    bool ignoreExtn = false;
    bool useThreads = false;    // threads slower on physical disks
    bool dryRun = false;        // -n scan and report but do not delete or hardline. 
    bool showAbsPath = false;

    // -- Duplicate file
    bool showSame = true;
    bool showDiff = false;
    bool showMiss = false;

    unsigned quiet = 0;             // 0=show output, 1=don't show dup/diff/miss, 2=suppress all output
    enum FileTypes { None, First, Second, Both };
    FileTypes showFiles = Both;
    FileTypes deleteFiles= None;
    
    unsigned sameCnt = 0;
    unsigned diffCnt = 0;
    unsigned missCnt = 0;
    unsigned skipCnt = 0;       // exclude and include filters rejected file.

    lstring separator = ", ";
    lstring preDivider = "";
    lstring postDivider = "\n";
    
    lstring preDup = "== ";
    lstring preMissing = "-- ";
    lstring preDiff = "!= ";

    // TODO - not yet implemented
    bool ignoreHardLinks = false;
    bool ignoreSoftLinks = false;
    bool hardlink = false;      // If dup, hardlink

private:
    lstring none;
    char code;

public:
    Command(char c);

    virtual  bool begin(StringList& fileDirList)  {
        return fileDirList.size() > 0;
    }

    virtual size_t add(const lstring& file) = 0;

    virtual bool end() {
        return true;
    }

    bool validFile(const lstring &name, const lstring &fullname);

    Command& share(const Command& other) {
        includeFilePatList = other.includeFilePatList;
        excludeFilePatList = other.excludeFilePatList;
        showFile = other.showFile;
        verbose = other.verbose;
        invert = other.invert;
        sameName = other.sameName;
        justName = other.justName;
        ignoreExtn = other.ignoreExtn;
        separator = other.separator;
        preDivider = other.preDivider;
        postDivider = other.postDivider;
        return *this;
    }

    static bool getFileTypes(FileTypes& fileTypes, const char *str);

    const char* absOrRel(const char* fullPath) const;
    const char* absOrRel(const string& fullPath) const;

    void showDuplicate(const lstring& filePath1, const lstring& filePath2);
    void showDifferent(const lstring& filePath1, const lstring& filePath2);
    void showMissing(bool have1, const lstring & filePath1, bool have2, const lstring& filePath2);
};


// ---------------------------------------------------------------------------

class DupFiles : public Command {
public:
    DupFiles() : Command('f') {}
    virtual  bool begin(StringList& fileDirList);
    virtual size_t add(const lstring& file);
    virtual bool end();

    void printPaths(const IntList& pathListIdx, const std::string& name);
};

