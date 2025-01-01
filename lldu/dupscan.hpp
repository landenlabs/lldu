//-------------------------------------------------------------------------------------------------
// File: dirScan.h    Author: Dennis Lang
//
// Desc: This class is used to recurse down directories to find duplicate files or missing files
//
// Usage::
//
//      The normal usage will be something like this:
//         DupScan dupScan;
//         StringSet nextDirList;
//         nextDirList.insert("");
//         unsigned level = 0;
//         while (dupScan.findDuplicates(level, fileDirList, nextDirList, *commandPtr)) {
//             level++;
//         }
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
#include "command.hpp"

#include <set>
typedef set<lstring> StringSet;

class DupScan {
public:
    Command& command;

    DupScan(Command& _command); 

    // Find duplicate files by name, size and hash_value
    //    Level used for diagnostics only.
    //    baseDirList contains 2 root directories to compare files by name, size and content.
    //    subDirList contains subdirectory paths to append to root, can be single emptry string.
    //    subDirList is replace with new set of directories for each depth level scanned.
    //    command - currently not used
    //
    //    returns - false when no more directories available.
    bool findDuplicates(unsigned level, const StringList& baseDirList, StringSet& subDirList) const;
    void done();

private:
    void scanFiles(unsigned level, const StringList& baseDirList, const StringSet& nextDirList) const;
    void getFiles(unsigned level, const StringList& baseDirList, const StringSet& nextDirList, set<lstring>& files) const;
    void getDirs(unsigned level, const StringList& baseDirList, const StringSet& nextDirList, StringSet& outDirList) const;
    void compareFiles(unsigned level, const StringList& baseDirList, const set<lstring>& files) const;


};

