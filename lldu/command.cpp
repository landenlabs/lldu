//-------------------------------------------------------------------------------------------------
//
// File: commands.cpp   Author: Dennis Lang  Desc: Process file scan
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

#include "ll_stdhdr.hpp"
#include "command.hpp"
#include "parseutil.hpp"  // fileMatches
#include "directory.hpp"
#include "hasher.hpp"

#include <assert.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <vector>




#ifdef HAVE_WIN
#include <direct.h> // _getcwd
#define chdir _chdir
#define getcwd _getcwd
#else
const size_t MAX_PATH = __DARWIN_MAXPATHLEN;
#endif

static const lstring EMPTY = "";
static char CWD_BUF[MAX_PATH];
static unsigned CWD_LEN = 0;

// ---------------------------------------------------------------------------
// Forward declaration
bool RunCommand(const char* command, DWORD* pExitCode, int waitMsec);

// ---------------------------------------------------------------------------
const string Q2 = "\"";
inline const string quote(const string& str) {
    return (str.find(" ") == string::npos) ? str : (Q2 + str + Q2);
}


#ifdef HAVE_WIN

// ---------------------------------------------------------------------------
std::string GetErrorMsg(DWORD error) {
    std::string errMsg;
    if (error != 0) {
        LPTSTR pszMessage;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&pszMessage,
            0, NULL);

        errMsg = pszMessage;
        LocalFree(pszMessage);
        int eolPos = (int)errMsg.find_first_of('\r');
        errMsg.resize(eolPos);
        errMsg.append(" ");
    }
    return errMsg;
}

// ---------------------------------------------------------------------------
// Has statics - not thread safe.
const char* RunExtension(std::string& exeName) {
    // Cache results - return previous match
    static const char* s_extn = NULL;
    static std::string s_lastExeName;
    if (s_lastExeName == exeName)
        return s_extn;
    s_lastExeName = exeName;

    /*
    static char ext[_MAX_EXT];
    _splitpath(exeName.c_str(), NULL, NULL, NULL, ext);

    if (ext[0] == '.')
        return ext;
    */

    // Expensive - search PATH for executable.
    char fullPath[MAX_PATH];
    static const char* s_extns[] = { NULL, ".exe", ".com", ".cmd", ".bat", ".ps" };
    for (unsigned idx = 0; idx != ARRAYSIZE(s_extns); idx++) {
        s_extn = s_extns[idx];
        DWORD foundPathLen = SearchPath(NULL, exeName.c_str(), s_extn, ARRAYSIZE(fullPath), fullPath, NULL);
        if (foundPathLen != 0)
            return s_extn;
    }

    return NULL;
}

// ---------------------------------------------------------------------------
bool RunCommand(const char* command, DWORD* pExitCode, int waitMsec) {
    std::string tmpCommand(command);
    /*
    const char* pEndExe = strchr(command, ' ');
    if (pEndExe == NULL)
        pEndExe = strchr(command, '\0');
    std::string exeName(command, pEndExe);

    const char* exeExtn = RunExtension(exeName);
    static const char* s_extns[] = { ".cmd", ".bat", ".ps" };
    if (exeExtn != NULL)
    {
        for (unsigned idx = 0; idx != ARRAYSIZE(s_extns); idx++)
        {
            const char* extn = s_extns[idx];
            if (strcmp(exeExtn, extn) == 0)
            {
                // Add .bat or .cmd to executable name.
                tmpCommand = exeName + extn + pEndExe;
                break;
            }
        }
    }
    */

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    // Start the child process.
    if (! CreateProcess(
        NULL,   // No module name (use command line)
        (LPSTR)tmpCommand.c_str(), // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory
        &si,            // Pointer to STARTUPINFO structure
        &pi)) {         // Pointer to PROCESS_INFORMATION structure
        DWORD err = GetLastError();
        if (pExitCode)
            *pExitCode = err;
        std::cerr << "Failed " << tmpCommand << " " << GetErrorMsg(err) << std::endl;
        return false;
    }

    // Wait until child process exits.
    DWORD createStatus = WaitForSingleObject(pi.hProcess, waitMsec);

    if (pExitCode) {
        *pExitCode = createStatus;
        if (createStatus == 0 && ! GetExitCodeProcess(pi.hProcess, pExitCode))
            *pExitCode = (DWORD) -1;
    }

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}
#else
// ---------------------------------------------------------------------------
const char* GetErrorMsg(DWORD error) {
    return std::strerror(error);
}

// ---------------------------------------------------------------------------
bool RunCommand(const char* command, DWORD* pExitCode, int waitMsec) {
    std::vector<char> buffer(128);
    std::string result;

    auto pipe = popen(command, "r"); // get rid of shared_ptr
    if (! pipe)
        throw std::runtime_error("popen() failed!");

    while (! feof(pipe)) {
        if (fgets(buffer.data(), 128, pipe) != nullptr)
            result += buffer.data();
    }

    auto rc = pclose(pipe);

    if (pExitCode != NULL) {
        *pExitCode = rc;
    }
    if (rc == EXIT_SUCCESS) { // == 0
    } else if (rc == EXIT_FAILURE) {  // EXIT_FAILURE is not used by all programs, maybe needs some adaptation.
    }
    return true;
}
#endif


// ---------------------------------------------------------------------------
static struct stat  print(const lstring& path, struct stat* pInfo) {
    struct stat info;
    int result = 0;

    if (pInfo == NULL) {
        pInfo = &info;
        result = stat(path, pInfo);
    }

    char timeBuf[128];
    errno_t err = 0;

    if (result == 0) {
        // err = ctime_s(timeBuf, sizeof(timeBuf), &pInfo->st_mtime);
        struct tm TM;
#ifdef HAVE_WIN
        err = localtime_s(&TM, &pInfo->st_mtime);
#else
        TM = *localtime(&pInfo->st_mtime);
#endif
        strftime(timeBuf, sizeof(timeBuf), "%a %d-%b-%Y %h:%M %p", &TM);
        if (err) {
            std::cerr << "Invalid file " << path << std::endl;
        } else {
#ifdef HAVE_WIN
            bool isSymLink = false;
#else
            bool isSymLink = S_ISLNK(pInfo->st_flags);
#endif
            std::cout << std::setw(8) << pInfo->st_size
                << " " << timeBuf << " "
                << std::setw(10) << pInfo->st_ino
                << (isSymLink ? " S" : " ")
                << "Links " << pInfo->st_nlink
                << " " << path
                << std::endl;
        }
    }
    return *pInfo;
}


// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
Command::Command(char c): code(c) {
    getcwd(CWD_BUF, sizeof(CWD_BUF));
    CWD_LEN = (unsigned)strlen(CWD_BUF);
    showAbsPath = false;
}

// ---------------------------------------------------------------------------
bool Command::validFile(const lstring& name, const lstring& fullname) {
  bool isValid =
      (!name.empty() && !ParseUtil::FileMatches(name, excludeFilePatList, false) &&
       ParseUtil::FileMatches(name, includeFilePatList, true) &&
       !ParseUtil::FileMatches(fullname, excludePathPatList, false) &&
       ParseUtil::FileMatches(fullname, includePathPatList, true));

    if (! isValid) {
        skipCnt++;
        
        if (verbose) {
            std::cerr << "Skipped:" << fullname;
            if (ParseUtil::FileMatches(name, excludeFilePatList, false))
                cerr << " exclude";
            if (!ParseUtil::FileMatches(name, includeFilePatList, true))
                cerr << " include";
            if (ParseUtil::FileMatches(fullname, excludePathPatList, false))
                cerr << " Exclude";
            if (!ParseUtil::FileMatches(fullname, includePathPatList, true))
                cerr << " Include";
            cerr << std::endl;
        }
    }
    return isValid;
}

// ---------------------------------------------------------------------------
const char*  Command::absOrRel(const char* fullPath) const {
    if (!showAbsPath && strncmp(fullPath, CWD_BUF, CWD_LEN) == 0)  
        return fullPath + CWD_LEN + 1;
    else
        return fullPath;
}
const char*  Command::absOrRel(const string& fullPath) const {
    if (!showAbsPath && strncmp(fullPath.c_str(), CWD_BUF, CWD_LEN) == 0)
        return fullPath.c_str() + CWD_LEN + 1;
    else
        return fullPath.c_str();
}


// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
map<std::string, IntList> fileList;
std::vector<std::string> pathList;
std::string lastPath;
unsigned lastPathIdx = 0;

// ---------------------------------------------------------------------------
bool DupFiles::begin(StringList& fileDirList) {
    fileList.clear();
    pathList.clear();
    lastPathIdx = 0;
    return true;
}

// ---------------------------------------------------------------------------
// Locate matching files which are not in exclude list.
// Locate duplicate files.
size_t DupFiles::add(const lstring& fullname) {
    size_t fileCount = 0;
    lstring name;
    DirUtil::getName(name, fullname);

    if (validFile(name, fullname)) {
        std::string path = fullname.substr(0, fullname.length() - name.length());
        if (lastPath != path) {
            if (lastPath.rfind(path, 0) == 0) {
                for (lastPathIdx--; lastPathIdx < pathList.size(); lastPathIdx--) {
                    lastPath = pathList[lastPathIdx];
                    if (lastPath == path) {
                        break;
                    }
                }
            } else {
                lastPathIdx = -1;
            }

            if (lastPathIdx >= pathList.size() ) {
                lastPathIdx = (unsigned)pathList.size();
                lastPath = path;
                pathList.push_back(lastPath);
            }

            string testPath = pathList[lastPathIdx] + name;
            if (testPath != fullname) {
                assert(false);
            }
        }
        fileList[name].push_back(lastPathIdx);
        fileCount = 1;
    }

    return fileCount;
}

class PathParts {
public:
    unsigned pathIdx;
    const string& name;
    PathParts(unsigned _pathIdx, const string& _name) :
        pathIdx(_pathIdx), name(_name) {}
};

void DupFiles::printPaths(const IntList& pathListIdx, const std::string& name) {
    for (unsigned plIdx = 0; plIdx < pathListIdx.size(); plIdx++) {
        lstring filePath = absOrRel(pathList[pathListIdx[plIdx]]) + name;
        if (verbose) {
            print(filePath, NULL);
        } else {
            if (plIdx != 0) std::cout << separator;
            std::cout << filePath;
        }
        if (ParseUtil::FileMatches(filePath, delDupPathPatList, false)) {
            DirUtil::deleteFile(dryRun, filePath);
        }
    }
}

// ---------------------------------------------------------------------------
bool DupFiles::end() {
    // typedef lstring HashValue;       // md5
    typedef uint64_t HashValue;         // xxHash64

    //  sameCnt
    //  diffCnt
    //  missCnt
    //  skipCnt

    if (justName && ignoreExtn) {
        lstring noExtn;
        std::map<lstring, std::vector<const string* >> noExtnList;       // TODO - make string& not string
        for (auto it = fileList.cbegin(); it != fileList.cend(); it++) {
            const string& fullname = it->first;
            DirUtil::getName(noExtn, fullname);
            DirUtil::removeExtn(noExtn, noExtn);
            noExtnList[noExtn].push_back(&fullname);
        }

        for (auto it = noExtnList.cbegin(); it != noExtnList.cend(); it++) {
            if (it->second.size() > 1) {
                sameCnt += it->second.size() - 1;
                uint outCnt = 0;
                for (auto itNames = it->second.cbegin(); itNames != it->second.cend(); itNames++) {
                    const IntList& pathListIdx = fileList[*(*itNames)];
                    if (outCnt++ == 0) 
                        std::cout << preDivider;
                    else 
                        std::cout << separator;
                    printPaths(pathListIdx, *(*itNames));
                }
                std::cout << postDivider;
            }
        }

    } else if (justName) {
        for (auto it = fileList.cbegin(); it != fileList.cend(); it++) {
            const IntList& pathListIdx = it->second;
            if (it->second.size() > 1) {
                sameCnt += it->second.size() - 1;
                std::cout << preDivider;
                printPaths(pathListIdx, it->first);
                std::cout << postDivider;
            }
        }
    } else if (sameName)  {
        std::map<HashValue, unsigned> hashDups;
        std::map<lstring, HashValue> fileHash;
        for (auto it = fileList.cbegin(); it != fileList.cend(); it++) {
            const IntList& pathListIdx = it->second;
            if (it->second.size() > 1) {
                hashDups.clear();
                fileHash.clear();

                for (unsigned plIdx = 0; plIdx < pathListIdx.size(); plIdx++) {
                    // std::cout << pathList[pathListIdx[plIdx]] << it->first << std::endl;
                    lstring fullPath = pathList[pathListIdx[plIdx]] + it->first;
                    // HashValue hashValue = Md5::compute(fullPath);
                    HashValue hashValue = Hasher::compute(fullPath);
                    hashDups[hashValue] = hashDups[hashValue] + 1;
                    fileHash[fullPath] = hashValue;
                }

                std::map<HashValue, std::vector<unsigned >> hashFileList;
                for (unsigned plIdx = 0; plIdx < pathListIdx.size(); plIdx++) {
                    // std::cout << pathList[pathListIdx[plIdx]] << it->first << std::endl;
                    unsigned plPos = pathListIdx[plIdx];
                    lstring fullPath = pathList[plPos] + it->first;
                    HashValue hashValue = fileHash[fullPath];
                    bool isDup = (hashDups[hashValue] != 1);
                    if (verbose) {
                        std::cout << (isDup ? preDup : preDiff) << fileHash[fullPath] << " ";
                        print(fullPath, NULL);
                        if (isDup) {
                            sameCnt++;
                            if (ParseUtil::FileMatches(fullPath, delDupPathPatList, false)) {
                                DirUtil::deleteFile(dryRun, fullPath);
                            }
                        } else 
                            diffCnt++;
                    } else if (isDup != invert) {
                        hashFileList[hashValue].push_back(plPos);
                    }
                }

                if (! verbose) {
                    for (auto hashFileListIter = hashFileList.cbegin(); hashFileListIter != hashFileList.cend(); hashFileListIter++) {
                        if (hashFileListIter->second.size() > 1) {
                            std::cout << preDivider;
                            const auto& matchList = hashFileListIter->second;
                            for (unsigned mIdx = 0; mIdx < matchList.size(); mIdx++) {
                                string fullPath = pathList[matchList[mIdx]] + it->first;
                                if (mIdx != 0) std::cout << separator;
                                std::cout << fullPath;
                                sameCnt++;
                                if (ParseUtil::FileMatches(fullPath, delDupPathPatList, false)) {
                                    DirUtil::deleteFile(dryRun, fullPath.c_str());
                                }
                            }
                            std::cout << postDivider;
                        }  
                    }
                }
            } else if (invert) {
                std::cout << preDivider;
                lstring fullPath = pathList[pathListIdx[0]] + it->first;
                std::cout << fullPath << postDivider;
            }
        }
    } else {
        // Compare all files by size and hash
        //  1. Create map of file length and name
        //  2. For duplicate file length - compute hash
        //  3. For duplicate hash print

        // 1. Create map of file length and name
        std::map<size_t, std::vector<PathParts >> sizeFileList;
        for (auto it = fileList.cbegin(); it != fileList.cend(); it++) {
            const IntList& pathListIdx = it->second;
            for (unsigned plIdx = 0; plIdx < pathListIdx.size(); plIdx++) {
                unsigned plPos = pathListIdx[plIdx];
                lstring fullPath = pathList[plPos] + it->first;
                size_t fileLen = DirUtil::fileLength(fullPath);
                fileLen = (fileLen != 0) ? fileLen : std::hash<std::string> {}(fullPath);
                const string& name = it->first;
                PathParts pathParts(plPos, name);
                sizeFileList[fileLen].push_back(pathParts);
            }
        }

        // 2. Compute hash on duplicate length files.
        std::map<HashValue, std::vector<const PathParts* >> hashFileList;
        for (auto sizeFileListIter = sizeFileList.cbegin(); sizeFileListIter != sizeFileList.cend(); sizeFileListIter++) {
            if ((sizeFileListIter->second.size() > 1) != invert) {
                const auto& sizeList = sizeFileListIter->second;
                for (unsigned sIdx = 0; sIdx < sizeList.size(); sIdx++) {
                    const PathParts& pathParts = sizeList[sIdx];
                    lstring fullPath = pathList[pathParts.pathIdx];
                    fullPath += pathParts.name;
                    // HashValue hashValue = Md5::compute(fullPath);
                    HashValue hashValue = Hasher::compute(fullPath);
                    hashFileList[hashValue].push_back(&pathParts);
                }
            }
        }

        // 3. Find duplicate hash
        for (auto hashFileListIter = hashFileList.cbegin(); hashFileListIter != hashFileList.cend(); hashFileListIter++) {
            if ((hashFileListIter->second.size() > 1) != invert) {
                sameCnt += hashFileListIter->second.size() - 1;
                if (showSame) std::cout << preDup;
                const auto& matchList = hashFileListIter->second;
                for (unsigned mIdx = 0; mIdx < matchList.size(); mIdx++) {
                    const PathParts& pathParts = *matchList[mIdx];
                    lstring fullPath = pathList[pathParts.pathIdx];
                    fullPath += pathParts.name;
                    if (verbose) {
                        std::cout << matchList.size() << " Hash " << hashFileListIter->first << " ";
                        print(fullPath, NULL);
                    } else if (showSame) {
                        if (mIdx != 0) std::cout << separator;
                        std::cout << absOrRel(fullPath);
                    }
                    if (ParseUtil::FileMatches(fullPath, delDupPathPatList, false)) {
                        DirUtil::deleteFile(dryRun, fullPath);
                    }
                }
                if (showSame)std::cout << postDivider;
            }
        }
    }
    return true;
}

//-------------------------------------------------------------------------------------------------
// [static] parse FileTypes from string
bool Command::getFileTypes(Command::FileTypes &fileTypes, const char *str) {
    // { None, First, Second, Both };
    bool valid = true;
    size_t slen = strlen(str);
    if (strncasecmp(str, "none", slen) == 0) {
        fileTypes = First;
    } else if (strncasecmp(str, "first", slen) == 0) {
        fileTypes = First;
    } else  if (strncasecmp(str, "second", slen) == 0) {
        fileTypes = Second;
    } else  if (strncasecmp(str, "both", slen) == 0) {
        fileTypes = Both;
    } else {
        valid = false;
    }

    return valid;
}


// ---------------------------------------------------------------------------
void Command::showDuplicate(const lstring& filePath1, const lstring& filePath2)  {
    sameCnt++;
    if (showSame) {
        std::cout << preDup;
        if (showFiles == Command::Both || showFiles == Command::First)
            std::cout << filePath1;
        if (showFiles == Command::Both)
            std::cout << separator;
        if (showFiles == Command::Both || showFiles == Command::Second)
            std::cout << filePath2;
        std::cout << postDivider;

        if (hardlink) {
            std::cerr << "Hardlink option not yet implemented\n";
            assert(true);  // TODO - implement hardlnk
            // DirUtil::deleteFile(dryRun, filePath2);
            // DirUtil::hardlink(filePath1, filePath2);
        } else if (deleteFiles != Command::None) {
            switch (deleteFiles) {
            case Command::None:
                break;
            case Command::First:
                DirUtil::deleteFile(dryRun, filePath1);
                break;
            case Command::Second:
                DirUtil::deleteFile(dryRun, filePath2);
                break;
            case Command::Both:
                DirUtil::deleteFile(dryRun, filePath1);
                DirUtil::deleteFile(dryRun, filePath2);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
void Command::showDifferent(const lstring& filePath1, const lstring& filePath2)  {
    diffCnt++;
    if (showDiff) {
        std::cout << preDiff;
        if (showFiles != Command::Second)
            std::cout << filePath1;
        if (showFiles != Command::None)
            std::cout << separator;
        if (showFiles != Command::First)
            std::cout << filePath2;
        std::cout << postDivider;
    }
}

// ---------------------------------------------------------------------------
void Command::showMissing(bool have1, const lstring& filePath1, bool have2, const lstring& filePath2)  {
    missCnt++;
    if (showMiss) {
        std::cout << preMissing;
        if (have1 != invert)
            std::cout << filePath1;
        else
            std::cout << filePath2;
        std::cout << postDivider;
    }
}
