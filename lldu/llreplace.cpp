//-------------------------------------------------------------------------------------------------
//
//  llreplace      9/16/2016       Dennis Lang
//
//  Regular expression file text replacement tool
//
//-------------------------------------------------------------------------------------------------
//
// Author: Dennis Lang - 2016
// https://landenlabs.com/
//
// This file is part of llreplace project.
//
// ----- License ----
//
// Copyright (c) 2016 Dennis Lang
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

// Project files
#include "ll_stdhdr.hpp"
#include "signals.hpp"
#include "parseutil.hpp"
#include "directory.hpp"
#include "filters.hpp"
#include "threader.hpp" // Defines CAN_THREAD

#include <assert.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <semaphore>
#include <regex>
#include <exception>
#include <chrono>   // Timing program execution

#ifdef HAVE_WIN
#define getcwd _getcwd

#include <sys/stat.h>
#include <direct.h> // _getcwd
#endif


// Helper types
typedef std::vector<lstring> StringList;
typedef unsigned int uint;

// Runtime options
std::regex fromPat;
enum FindMode { FROM, FROM_TILL, FROM_UNTIL };
FindMode findMode = FROM;
std::regex tillPat;
std::regex untilPat;
lstring toPat;

// TODO - should this be moved into Range Filter, also allow array of these
std::regex beginPat;
std::regex ignorePat;
std::regex endPat;

lstring backupDir;
PatternList includeFilePatList;
PatternList excludeFilePatList;
PatternList includePathPatList;
PatternList excludePathPatList;
StringList fileDirList;
lstring outFile;
lstring printPat;

bool isVerbose = false;
bool doLineByLine = false;      // What is this for ?, should it be a switch
bool showPattern = false;
bool inverseMatch = false;
bool dryRun = false;
bool canForce = false;          // Can update read-only file.
bool progress = true;           // Show progress
bool doReplace = false;
bool runWithThreads = false;
bool binaryOkay = false;
// bool recurse = true;
// static std::binary_semaphore lockOutput{0}; // Single thread can output to console.
static std::counting_semaphore lockOutput{1};

uint optionErrCnt = 0;
uint patternErrCnt = 0;
lstring printPosFmt = "%r/%f(%o) %l\n";
lstring cwd;    // current working directory

const char EXTN_CHAR = '.';
const char EOL_CHR = '\n';
const char* EOL_STR = "\n";
const size_t KB = 1024;
const size_t MB = KB * KB;
const size_t GB = MB * KB;
const size_t MAX_FILE_SIZE_DEF = MB * 200;
size_t maxFileSize = MAX_FILE_SIZE_DEF;

#if 0
    // Sample replace fragment.
    std::regex specialCharRe("[*?-]+");
    std::regex_constants::match_flag_type flags = std::regex_constants::match_default;
    title = std::regex_replace(title, specialCharRe, "_", flags);
    std::replace(title.begin(), title.end(), SLASH_CHR, '_');

#endif

#if 1
std::regex_constants::match_flag_type rxFlags =
    std::regex_constants::match_flag_type(
        std::regex_constants::match_default
        + std::regex_constants::extended
    );
#else
std::regex_constants::match_flag_type rxFlags =
    std::regex_constants::match_flag_type(
        std::regex_constants::match_default
        + std::regex_constants::match_not_eol
        + std::regex_constants::match_not_bol);
#endif


Filter nopFilter;
LineFilter lineFilter;
BufferFilter bufferFilter;
Filter* pFilter = &nopFilter;

unsigned long g_regSearchCnt = 0;
unsigned long g_fileCnt = 0;
unsigned long g_binaryCnt = 0;



// ---------------------------------------------------------------------------
// Return true if inPath (filename part) matches pattern in patternList
bool FileMatches(const lstring& inName, const PatternList& patternList, bool emptyResult) {
    if (patternList.empty() || inName.empty())
        return emptyResult;

    for (size_t idx = 0; idx != patternList.size(); idx++)
        if (std::regex_match(inName.begin(), inName.end(), patternList[idx]))
            return true;

    return false;
}
// ---------------------------------------------------------------------------
lstring getPartDir(const char* filepath) {
    lstring result = filepath;
    size_t endDir = result.find_last_of(Directory_files::SLASH);
    if (endDir != string::npos)
        result = result.substr(0, endDir);
    return result;
}
// ---------------------------------------------------------------------------
lstring getPartName(const char* filepath) {
    lstring result = filepath;
    size_t endDir = result.find_last_of(Directory_files::SLASH);
    if (endDir != string::npos)
        result = result.substr(endDir + 1);
    size_t endName = result.find_last_of(EXTN_CHAR);
    if (endName != string::npos)
        result.resize(endName);
    return result;
}
// ---------------------------------------------------------------------------
lstring getPartExt(const char* filepath) {
    lstring result = filepath;
    size_t pos = result.find_last_of(EXTN_CHAR);
    return result.substr(pos);
}
// ---------------------------------------------------------------------------
lstring parts(const char* filepath, bool dir, bool name, bool ext) {
    // #include <filesystem>
    // std::filesystem::path pathParts = filepath;

    lstring result;
    if (dir) {
        result += getPartDir(filepath);
    }
    if (name) {
        result += getPartName(filepath);
    }
    if (ext) {
        result += getPartExt(filepath);
    }
    return result;
}

// ---------------------------------------------------------------------------
void printParts(
    const char* customFmt,
    const char* filepath,
    size_t fileOffset,
    size_t matchLen,
    const char* matchPtr, 
    const char* begLinePtr) {

    const int NONE = 12345;
    lstring itemFmt;
    size_t lineLen;
    // char c;

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

            // Default:  "%r/%f(%o) %l\n";
            // The %X  can be %<width>.<maxWidth>X  as in %20.20s 
            //  %s=entire file path
            //  %p=just directory path
            //  %r=relative directory path
            //  %n=filename only (no extension)
            //  %e=extension
            //  %f=filename with extension
            //  %o=offset into file where match found
            //  %z=match length
            //  %m=matched string
            //  %l=matched line
            //  %t=TO_REPLACEMENT  

            switch (c) {
            case 's':
                itemFmt += "s";
                printf(itemFmt, filepath);
                break;
            case 'p':
                itemFmt += "s";
                printf(itemFmt, parts(filepath, true, false, false).c_str());
                break;
            case 'r':   // relative path
                itemFmt += "s";
                printf(itemFmt, parts(filepath, true, false, false).replaceStr(cwd, "").c_str());
                break;
            case 'n':
                itemFmt += "s";
                printf(itemFmt, parts(filepath, false, true, false).c_str());
                break;
            case 'e':
                itemFmt += "s";
                printf(itemFmt, parts(filepath, false, false, true).c_str());
                break;
            case 'f':
                itemFmt += "s";
                printf(itemFmt, parts(filepath, false, true, true).c_str());
                break;
            case 'o':
                itemFmt += "lu";    // unsigned long formatter
                printf(itemFmt, fileOffset);
                break;
            case 'z':
                itemFmt += "lu";    // unsigned long formatter
                printf(itemFmt, matchLen);
                break;
            case 'm':
                itemFmt += "s";
                c = matchPtr[matchLen];
                ((char*)matchPtr)[matchLen] = '\0';
                printf(itemFmt, matchPtr);
                ( (char*)matchPtr )[matchLen] = c;
                break;
            case 'l':
                lineLen = strcspn(begLinePtr, EOL_STR);

#define COLORIZE 1
#if COLORIZE
                printf("%.*s", (int)(matchPtr-begLinePtr), begLinePtr);
                std::cerr << Colors::colorize("_Y_");
                printf("%.*s", (int)matchLen, matchPtr);
                std::cerr << Colors::colorize("_X_");
                printf("%.*s", int(lineLen - matchLen - ( matchPtr - begLinePtr )), matchPtr+matchLen);
#else
                itemFmt += "s";
                printf(itemFmt, lstring(begLinePtr, lineLen).c_str());
#endif
                break;
                /* 
            case 't':   // match convert using printPat
                itemFmt += "s";
                if (printPat.length() == 0) {
                    printf("Missing -printPat=pattern");
                } else {
                    printf(itemFmt, std::regex_replace(matchPtr, fromPat, printPat, rxFlags).c_str());
                }
                break;
                */
            default:
                putchar(c);
                break;
            }
            fmt++;
        }
    }
}

// ---------------------------------------------------------------------------
const char* strchrRev(const char* sPtr, char want) {
    while (*sPtr != want) {
        sPtr--;
    }
    return sPtr;
}

// ---------------------------------------------------------------------------
void fileProgress(const char* filePath) {
     g_fileCnt++;
    if ((g_fileCnt % 100) == 0)
        std::cerr << "\r" << "Files:" << g_fileCnt << " ";
}

// ---------------------------------------------------------------------------
bool isBinary(Buffer& buffer, struct stat& filestat, const char* fullname) {
    if (binaryOkay)
        return false;
    
    bool isBinary = false;  // filestat.st_mode & S_IXUSR
    bool isUtf16 = false;
    if (buffer.size() > KB) {
        size_t goodCnt = 0;
        size_t nullCnt = 0;
        unsigned char* cPtr = (unsigned char*)buffer.data();
        unsigned char* endPtr = cPtr + KB;
        while (cPtr < endPtr) {
            unsigned char c = *cPtr++;
            if (c >= 32 && c < 128)
                goodCnt++;
            else if (c == '\0')
                nullCnt++;
        }
        isBinary = goodCnt < (KB/2);
        isUtf16 = (nullCnt > goodCnt && nullCnt - goodCnt < KB/50); // utf16 is often [null, ascii]... so null cnt is similar to good cnt
    }
    if (isBinary) {
        g_binaryCnt++;
        if (isVerbose)
            std::cerr << "Skipping " << (isUtf16 ? "UTF16 ": "BINARY ") << fullname << std::endl;
    }
    return isBinary;
}

// Forward declaration
unsigned FindLineGrep(const char* filepath);

// ---------------------------------------------------------------------------
// Find 'fromPat' in file
unsigned FindFileGrep(const char* filepath) {
    lstring         filename;
    ifstream        in;
    ofstream        out;
    struct stat     filestat;
    uint            matchCnt = 0;


    try {
        if (stat(filepath, &filestat) != 0)
            return 0;
        
        if (filestat.st_size > maxFileSize) {
            Colors::showError("File too large ", filepath, " ", filestat.st_size);
            return 0;
            // return FindLineGrep(filepath);
        }
        
        in.open(filepath);
        if (in.good() && filestat.st_size > 0 && S_ISREG(filestat.st_mode)) {

            try {
                Buffer buffer(filestat.st_size+1);
                buffer[0] = EOL_CHR;
                streamsize inCnt = in.read(buffer.data() + 1, buffer.size()-1).gcount();
                in.close();
                
                if (isBinary(buffer, filestat, filepath))
                    return 0;
                    
                std::match_results <const char*> match;
                const char* begPtr = (const char*)buffer.data() + 1;
                const char* endPtr = begPtr + inCnt;
                size_t off = 0;
                pFilter->init(buffer.data());
                


                fileProgress(filepath);   // g_fileCnt++;
                while (std::regex_search(begPtr, endPtr, match, fromPat, rxFlags)) {
                    g_regSearchCnt++;
                    // for (auto group : match)
                    //    std::cout << group << endl;
                    
                    size_t pos = match.position();
                    size_t len = match.length();
                    // size_t numMatches = match.size();
                    // std::string matchedStr = match.str();
                    // std::string m0 = match[0];
                    
                    off += pos;
                    
                    if (pFilter->valid(off, len)) {
                        if (! inverseMatch) {
                            if (matchCnt == 0)
                                lockOutput.acquire();
                            // printParts(printPosFmt, filepath, off, len, lstring(begPtr + pos, len), strchrRev(begPtr + pos, EOL_CHR) +1);
                            printParts(printPosFmt, filepath, off, len, begPtr + pos, strchrRev(begPtr + pos, EOL_CHR) +1);
                            matchCnt++;
                        }
                    }
                    
                    begPtr += pos + len;
                }
            } catch (exception ex) {
                int err = errno;
                Colors::showError("File read exception on ", filepath, " ", strerror(err));
            }
           
        } else if (in.bad()) {
            Colors::showError("Unable to open ", filepath);
        }
    } catch (exception ex) {
        Colors::showError(ex.what(), " Parsing error in file:",  filepath);
    }
    
    if (matchCnt != 0)
        lockOutput.release();
    
    return inverseMatch ? (matchCnt > 0 ? 0 : 1) : matchCnt;
}

// ---------------------------------------------------------------------------
// Find 'fromPat' in file
unsigned FindLineGrep(const char* filepath) {
    lstring         filename;
    ifstream        in;
    ofstream        out;
    struct stat     filestat;
    uint            matchCnt = 0;


    try {
        if (stat(filepath, &filestat) != 0)
            return 0;

        in.open(filepath);
        if (in.good() && filestat.st_size > 0 && S_ISREG(filestat.st_mode)) {
            // std::match_results <std::string> match;
            std::smatch match;
            size_t off = 0;

            Buffer buffer(512);
            pFilter->init(buffer.data());
            std::string lineBuffer;

            fileProgress(filepath);   // g_fileCnt++;
            while (getline(in, lineBuffer)) {
                if (std::regex_search(lineBuffer.cbegin(), lineBuffer.cend(), match, fromPat, rxFlags))   {
                    g_regSearchCnt++;
                    // for (auto group : match)
                    //    std::cout << group << endl;

                    size_t pos = match.position();
                    size_t len = match.length();
                    // size_t numMatches = match.size();
                    // std::string matchedStr = match.str();
                    // std::string m0 = match[0];

                    off += pos;

                    if (pFilter->valid(off, len)) {
                        if (! inverseMatch) {
                            if (matchCnt == 0)
                                lockOutput.acquire();
                            
                            printParts(printPosFmt, filepath, off, len, lineBuffer.c_str()+pos, lineBuffer.c_str());
                            if (doReplace) {
                                string result;
                                std::regex_replace(std::back_inserter(result), lineBuffer.begin(), lineBuffer.end(), fromPat, toPat, rxFlags);
                                std::cout << "TO=" << result << std::endl;
                            }
                            matchCnt++;
                        }
                    }
                }
            }
            
        } else if (!in.good()) {
            Colors::showError("Unable to open ", filepath);
        }
    } catch (exception ex) {
        Colors::showError(ex.what(), " Parsing error in file:",  filepath);
    }

    if (matchCnt != 0)
        lockOutput.release();
    
    return inverseMatch ? (matchCnt > 0 ? 0 : 1) : matchCnt;
}

// ---------------------------------------------------------------------------
// Find fromPat and Replace with toPat in filepath.
//  inFilepath = full file name path
//  outfilePath = full file name paht, can be same as inFilepath
//  backupToName = name only part
bool ReplaceFile(const lstring& inFilepath, const lstring& outFilepath, const lstring& backupToName) {
    ifstream        in;
    ofstream        out;
    struct stat     filestat;

    try {
        if (stat(inFilepath, &filestat) != 0)
            return false;
        
        if (filestat.st_size > maxFileSize) {
            Colors::showError("File too large ", inFilepath, " ", filestat.st_size);
            return 0;
        }
        
        in.open(inFilepath);
        if (in.good() && filestat.st_size > 0 && S_ISREG(filestat.st_mode)) {
            fileProgress(inFilepath);   // g_fileCnt++;
            Buffer buffer(filestat.st_size+1);
            // buffer.resize(filestat.st_size);
            streamsize inCnt = in.read(buffer.data(), buffer.size()).gcount();
            in.close();

            if (isBinary(buffer, filestat, inFilepath))
                return 0;
            
            std::match_results <const char*> match;
            std::match_results <const char*> matchEnd;
            const char* begPtr = (const char*)buffer.data();
            const char* endPtr = begPtr + inCnt;
            pFilter->init(buffer.data());

            // WARNING - LineFilter only validates first match and not multiple replacements.
            if (std::regex_search(begPtr, endPtr, match, fromPat, rxFlags)
                && pFilter->valid(match.position(), match.length())) {
                g_regSearchCnt++;
                if (! backupDir.empty()) {
                    lstring backupFull;
                    rename(inFilepath, DirUtil::join(backupFull, backupDir, backupToName));
                }

                ostream* outPtr = &cout;
                if (outFilepath != "-") {
                    if (canForce)
                        DirUtil::makeWriteableFile(outFilepath, nullptr);
                    out.open(outFilepath);
                    if (out.is_open()) {
                        outPtr = &out;
                    } else {
                        Colors::showError(strerror(errno), ", Unable to write to ", outFilepath);
                        return false;
                    }
                }

                // TODO - support tillPat and untilPat
                // Compute region of match and replace it.
                size_t offset;
                switch (findMode) {
                case FROM:
                    // TODO - support LineFilter validation.
                    std::regex_replace(std::ostreambuf_iterator<char>(*outPtr),
                        begPtr,
                        endPtr,
                        fromPat, toPat, rxFlags);
                    break;
                case FROM_TILL:
                    do {
                        outPtr->write(begPtr, match.position());
                        offset = match.position() +  match.length();
                        if (std::regex_search(begPtr + offset, endPtr, matchEnd, tillPat, rxFlags)) {
                            (*outPtr) << toPat;
                            offset += matchEnd.position() + matchEnd.length();
                        }
                        begPtr += offset;
                    } while (std::regex_search(begPtr, endPtr, match, fromPat, rxFlags));
                    outPtr->write(begPtr, endPtr - begPtr);
                    break;
                case FROM_UNTIL:
                    do {
                        outPtr->write(begPtr, match.position());
                        offset = match.position() +  match.length();
                        if (std::regex_search(begPtr + offset, endPtr, matchEnd, tillPat, rxFlags)) {
                            (*outPtr) << toPat;
                            offset += matchEnd.position();
                        }
                        begPtr += offset;
                    } while (std::regex_search(begPtr, endPtr, match, fromPat, rxFlags));
                    outPtr->write(begPtr, endPtr - begPtr);
                    break;
                }

                if (out.is_open()) {
                    out.close();
                }
                return true;
            }
        } else if (!in.good()) {
            Colors::showError(strerror(errno), ", Unable to open ", inFilepath);
        }
    } catch (exception ex) {
        Colors::showError(ex.what(), ", Error in file:", inFilepath);
    }
    return false;
}

// ---------------------------------------------------------------------------
static size_t ReplaceFile(const lstring& inFullname) {
    size_t fileCount = 0;
    lstring name;
    DirUtil::getName(name, inFullname);

    if (! name.empty()
        && ! FileMatches(name, excludeFilePatList, false)
        && FileMatches(name, includeFilePatList, true)) {
        if (doReplace && !dryRun) {
            string outFullname = (outFile.length() != 0) ? outFile : inFullname;
            if (ReplaceFile(inFullname, outFullname, name)) {
                fileCount++;
                // printParts(printPosFmt, fullname, 0, 0, "");
            }
        } else {
            unsigned matchCnt;
            if (doLineByLine) {
                matchCnt = FindLineGrep(inFullname);
            } else {
                matchCnt = FindFileGrep(inFullname);
            }
            if (isVerbose) {
                std::cerr << "Match Found=" << matchCnt  << " in " << inFullname << std::endl;
            }
            if (matchCnt != 0) {
                fileCount++;
                // printParts(printPosFmt, fullname, 0, 0, "");
            }
        }
    }

    return fileCount;
}

// ---------------------------------------------------------------------------
// Optional thread support

#ifdef CAN_THREAD
class ReplaceJob : public Job {
public:
    lstring fullname;
    static std::atomic<size_t> result;
    
    ReplaceJob(const lstring& _fullname) : fullname(_fullname) {
    }
    ~ReplaceJob() { }
    virtual void run() {
        result += ReplaceFile(fullname);
    }
    virtual void dump() {
        std::cerr << fullname << std::endl;
    }
};
std::atomic<size_t> ReplaceJob::result = 0;
#endif



static size_t ThreadReplaceFile(const lstring& inFullname) {
#ifdef CAN_THREAD
    if (runWithThreads) {
        Threader::runIt(new ReplaceJob(inFullname));
        return 0;
    }
#endif
    return ReplaceFile(inFullname);
}
static void ReplaceFilesInit() {
#ifdef CAN_THREAD
    if (runWithThreads) {
        std::cerr << "Running with " << Threader::maxThreads << " threads\n";
        Threader::init();
    }
#endif
}
static size_t ReplaceFilesDone() {
#ifdef CAN_THREAD
    if (runWithThreads) {
        Threader::waitForAll();
        return ReplaceJob::result;
    }
#endif
    return 0;
}

// ---------------------------------------------------------------------------
static size_t ReplaceFiles(const lstring& dirname) {
    Directory_files directory(dirname);
    lstring fullname;

    size_t fileCount = 0;

    struct stat filestat;
    try {
        if (stat(dirname, &filestat) == 0 && S_ISREG(filestat.st_mode)) {
            fileCount += ThreadReplaceFile(dirname);
        }
    } catch (exception ex) {
        // Probably a pattern, let directory scan do its magic.
    }

    while (!Signals::aborted && directory.more()) {
        directory.fullName(fullname);
        if (directory.is_directory()) {
            if(! FileMatches(fullname, excludePathPatList, false)
               && FileMatches(fullname, includePathPatList, true)) {
            fileCount += ReplaceFiles(fullname);
           }
        } else if (fullname.length() > 0) {
            fileCount += ThreadReplaceFile(fullname);
        }
    }

    return fileCount;
}

// ---------------------------------------------------------------------------
template<typename TT>
std::string stringer(const TT& value) {
    return string(value);
}
template<typename TT, typename ... Args >
std::string stringer(const TT& value, const Args& ... args) {
    return string(value) + stringer(args...);
}

// ---------------------------------------------------------------------------
void showHelp(const char* argv0) {
    const char* helpMsg =
        "  Dennis Lang v2.6 (LandenLabs.com) " __DATE__  "\n"
        "\nDes: Replace text in files\n"
        "Use: llreplace [options] directories...\n"
        "\n"
        "_P_Main options:_X_\n"
        "   -_y_from=<regExpression>          ; Pattern to find\n"
        "   -_y_till<regExpression>           ;   Optional end pattern to find\n"
        "   -_y_until<regExpression>          ;   Optional end pattern to find\n"
        "   -_y_to=<regExpression or string>  ; Optional replacment \n"
        "   -_y_backupDir=<directory>         ; Optional Path to store backup copy before change\n"
        "   -_y_out= - | outfilepath          ; Optional alternate output, default is input file \n"
        "\n"
        "   -_y_includeFile=<filePattern>     ; Include files by regex match \n"
        "   -_y_excludeFile=<filePattern>     ; Exclude files by regex match \n"
        "   -_y_IncludePath=<pathPattern>     ; Include path by regex match \n"
        "   -_y_ExcludePath=<pathPattern>     ; Exclude path by regex match \n"
        "   -_y_range=beg,end                 ; Optional line range filter \n"

        "\n"
        "   directories...                 ; Directories to scan\n"
        "\n"
        "_P_Other options:_X_\n"
        "   -_y_ignoreCase                    ; Next pattern set to ignore case \n"
        "   -_y_force                         ; Allow updates on read-only files \n"
        "   -_y_no                            ; Dry run, show changes if replacing \n"
        "   -_y_inverse                       ; Invert Search, show files not matching \n"
        "   -_y_maxFileSize=<#MB>             ; Max file size MB, def= 200 \n"
        "   -_y_binary                        ; Include binary files \n"
#ifdef CAN_THREAD
        "   -_y_threads                       ; Search/Replace using 20 threads \n"
        "   -_y_threads=<#threads>            ; Search/Replace using threads \n"
#endif
        "\n"
        "_P_PrintFmt:_X_ \n"
        "   -_y_printFmt=' %r/%f(%o) %l\\n'    ; Printf format to present match \n"
        "    Each special character can include minWidth.maxWidth\n"
        "       %10s     = pad out to 10 wide minimum\n"
        "       %10.10s  = pad out to 10 wide min and clip to max of 10 \n"
        "       Without min or max, use entire value\n"
        "\n"
        "       %s = entire file path \n"
        "       %p = just directory path \n"
        "       %r = relative directory path \n"
        "       %n = file name only (no extension) \n"
        "       %e = extension \n"
        "       %f = filename with extension \n"
        "       %o = offset into file where match found \n"
        "       %z = match length \n"
        "       %m = matched string \n"
        "       %l = matched line  (colorized output)  \n"
        "  ex: -%_y_printFmt='%20.20f %08o\\n'  \n"
        "  Filename padded to 20 characters, max 20, and offset 8 digits leading zeros.\n"
        "\n"
        "_p_NOTES:\n"
        "   . (dot) does not match \\r \\n,  you need to use [\\r\\n] or  (.|\\r|\\n)* \n"
        "   Use lookahead for negative condition with dot, ex: \"(?!</section).\"  Full patter below\n"
        "   Use single quotes to wrap from and/or to patterns if they use special characters\n"
        "   like $ dollar sign to prevent shell from interception it.\n"
        "\n"
        "_p_Examples\n"
        " Search only, show patterns and defaults showing file and match:\n"
        "  llreplace -_y_from='Copyright' '-_y_include=*.java' -_y_print='%r/%f\\n' src1 src2\n"
        "  llreplace -_y_from='Copyright' '-_y_include=*.java' -_y_include='*.xml' -_y_print='%s' -_y_inverse src res\n"
        "  llreplace '-_y_from=if [(]MapConfigInfo.DEBUG[)] [{][\\r\\n ]*Log[.](d|e)([(][^)]*[)];)[\\r\\n ]*[}]'  '-_y_include=*.java' -_y_range=0,10 -_y_range=20,-1 -_y_printFmt='%f %03d: ' src1 src2\n"
        "  llreplace -_y_printFmt=\"%m\\n\" -_y_from=\"<section id='trail-stats'>((?!</section).|\\r|\\n)*</section>\" \n"
        "\n"
        " _P_Search and replace in-place:_X_\n"
        "  llreplace '-_y_from=if [(]MapConfigInfo.DEBUG[)] [{][\\r\\n ]*Log[.](d|e)([(][^)]*[)];)[\\r\\n ]*[}]' '-_y_to=MapConfigInfo.$1$2$3' '-_y_include=*.java' src\n"
        "  llreplace '-_y_from=<block>' -till='</block>' '-_y_to=' '-_y_include=*.xml' res\n"
        "\n";

    std::cerr << Colors::colorize(stringer("\n_W_", argv0, "_X_").c_str()) << Colors::colorize(helpMsg);
}

// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    Signals::init();
    ParseUtil parser;

    if (argc == 1) {
        showHelp(argv[0]);
    } else {
        bool doParseCmds = true;
        string endCmds = "--";
        for (int argn = 1; argn < argc; argn++) {
            if (*argv[argn] == '-' && doParseCmds) {
                lstring argStr(argv[argn]);
                Split cmdValue(argStr, "=", 2);
                if (argStr.find("=") != string::npos && cmdValue.size() >= 1) {
                    lstring cmd = cmdValue[0];
                    lstring value = cmdValue[1];

                    const char* cmdName = cmd + 1;
                    if (cmd.length() > 2 && *cmdName == '-')
                        cmdName++;  // allow -- prefix on commands

                    switch (*cmdName) {
                    case 'b':   // backup path
                        if (parser.validOption("backupDir", cmdName, false))
                            backupDir = value;
                        else if (parser.validOption("begin", cmdName, false)) {
                            ParseUtil::convertSpecialChar(value);
                            beginPat = parser.getRegEx(value);
                            // doLineByLine = value.find("\n") == std::string::npos;
                        }
                        break;
                    case 'f':   // from=<pat>
                        if (parser.validOption("from", cmdName)) {
                            fromPat = parser.getRegEx(ParseUtil::convertSpecialChar(value));
                            findMode = FROM;
                            // doLineByLine = value.find("\n") == std::string::npos;
                        }
                        break;

                    case 'i': // -ignore=<pattern>
                        if (parser.validOption("ignore", cmdName, false)) {
                            ParseUtil::convertSpecialChar(value);
                            ignorePat = parser.getRegEx(value);
                            // doLineByLine = value.find("\n") == std::string::npos;
                        } else {  // includeFile=<pat>
                            parser.validPattern(includeFilePatList, value, "includeFile", cmdName);
                        }
                        break;
                    case 'e':   // excludeFile=<pat>
                        if (parser.validPattern(excludeFilePatList, value, "excludeFile", cmdName, false)) {
                        } else  if (parser.validOption("end", cmdName)) {
                            ParseUtil::convertSpecialChar(value);
                            endPat = parser.getRegEx(value);
                            // doLineByLine = value.find("\n") == std::string::npos;
                        }
                        break;
                    case 'E':   // ExcludePath=<pat>
                        parser.validPattern(excludePathPatList, value, "ExcludePath", cmdName);
                        break;
                    case 'I':   // IncludePath=<pat>
                        parser.validPattern(includePathPatList, value, "IncludePath", cmdName);
                        break;
                        case 'm':
                            if (parser.validOption("maxFileSizeMB", cmdName))  {
                                maxFileSize = atoi(value) * MB;
                                maxFileSize = std::max((size_t)512, std::min(maxFileSize, GB*32));
                                std::cerr << "MaxFileSizeMB=" << maxFileSize/MB << std::endl;
                            }
                            break;
                    case 'r':   // range=beg,end
                        if (parser.validOption("range", cmdName))  {
                            char* nPtr;
                            size_t n1 = strtoul(value, &nPtr, 10);
                            size_t n2 = strtoul(nPtr + 1, &nPtr, 10);
                            if (n1 <= n2) {
                                // TODO - should BeginPat, IgnorePat, EndPat be part of range filter
                                pFilter = &bufferFilter;
                                lineFilter.zones.push_back(Zone(n1, n2));
                                bufferFilter.zones.push_back(Zone(n1, n2));
                            }
                        }
                        break;

                    case 'o':   // alternate output
                        if (parser.validOption("out", cmdName))
                            outFile = value;
                        break;
                    case 'p':
                        if (parser.validOption("printFmt", cmdName, false)) {
                            printPosFmt = value;
                            ParseUtil::convertSpecialChar(printPosFmt);
                        } else if (parser.validOption("printPat", cmdName)) {
                            printPat = value;
                            ParseUtil::convertSpecialChar(printPat);
                        }
                        break;

                    case 't':   // to=<pat>
                        if (parser.validOption("till", cmdName, false))  {
                            ParseUtil::convertSpecialChar(value);
                            tillPat = parser.getRegEx(value);
                            findMode = FROM_TILL;
                            doLineByLine = true;
                        } else if (parser.validOption("to", cmdName, false))  {
                            toPat = value;
                            ParseUtil::convertSpecialChar(toPat);
                            doReplace = true;
#ifdef CAN_THREAD
                        } else if (parser.validOption("threads", cmdName))  {
                            runWithThreads = true;
                            Threader::maxThreads = (ThreadCnt)atoi(value);
#endif
                        }
                        break;

                    case 'u':   // until=<pat>
                        if (parser.validOption("until", cmdName)) {
                            ParseUtil::convertSpecialChar(value);
                            untilPat = parser.getRegEx(value);
                            findMode = FROM_UNTIL;
                            doLineByLine = true;
                        }
                        break;

                    default:
                        parser.showUnknown(argStr);
                        break;
                    }
                } else {
                    if (endCmds == argv[argn]) {
                        doParseCmds = false;
                    } else {
                        const char* cmdName = argStr + 1;
                        if (argStr.length() > 2 && *cmdName == '-')
                            cmdName++;  // allow -- prefix on commands
                        switch (*cmdName) {
                        case 'b':
                            binaryOkay = parser.validOption("binary", cmdName);
                            break;
                        case 'f':
                            canForce = parser.validOption("force", cmdName);
                            break;
                        case 'i':
                            if (parser.validOption("ignoreCase", cmdName, false)) {
                                parser.ignoreCase = true;
                            } else 
                                inverseMatch = parser.validOption("inverse", cmdName);
                            break;
                        case 'n':
                            doLineByLine = dryRun = parser.validOption("no", cmdName);
                            break;
#ifdef CAN_THREAD
                        case 't':
                            runWithThreads = parser.validOption("threads", cmdName);
                            break;
#endif
                        case 'v':
                            isVerbose = parser.validOption("verbose", cmdName);
                            break;
                        default:
                            parser.showUnknown(argStr);
                            break;
                        }
                    }
                }
            } else {
                // Store file directories
                fileDirList.push_back(argv[argn]);
            }
        }

        if (parser.parseArgSet.count("from") == 0) {
            Colors::showError("Missing -from='pattern'");
            return 0;
        }

        if (patternErrCnt == 0 && optionErrCnt == 0 && fileDirList.size() != 0) {
            // Get starting timepoint
            auto start = std::chrono::high_resolution_clock::now();
            ReplaceFilesInit();
            
            char cwdTmp[256];
            cwd = getcwd(cwdTmp, sizeof(cwdTmp));
            cwd += Directory_files::SLASH;

            if (! toPat.empty() && pFilter != &nopFilter) {
                Colors::showError("\a\nRange filter does not work for replacement only searching\a");
            }
            if (pFilter != &nopFilter) {
                pFilter = doLineByLine ? ((Filter*)&lineFilter) : ((Filter*)&bufferFilter);
            }

            size_t fileMatchCnt = 0;
            if (fileDirList.size() == 1 && fileDirList[0] == "-") {
                string filePath;
                while (std::getline(std::cin, filePath)) {
                    fileMatchCnt += ReplaceFiles(filePath);
                }
            } else {
                for (auto const& filePath : fileDirList) {
                    fileMatchCnt += ReplaceFiles(filePath);
                }
            }

            fileMatchCnt += ReplaceFilesDone();
            
            // Get ending timepoint
            auto stop = std::chrono::high_resolution_clock::now();
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(stop - start);
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(stop - start);
            auto milli = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

            std::cerr << "\n";
            if (minutes.count() > 5) {
                std::cerr << "Elapsed " << minutes.count() << " minutes" << endl;
            } else if (seconds.count() > 5) {
                std::cerr << "Elapsed " << seconds.count() << " seconds" << endl;
            } else {
                std::cerr << "Elapsed " << milli.count() << " milliSeconds" << endl;
            }
            std::cerr << "FilesChecked= " << g_fileCnt << endl;
            std::cerr << "BinarySkipped=" << g_binaryCnt << endl;
            std::cerr << "FilesMatched= " << fileMatchCnt << endl;
            if (toPat.empty() || doLineByLine) {
                std::cerr << "LinesMatched= " <<  g_regSearchCnt << endl;
            }

        } else {
            showHelp(argv[0]);
        }

        std::cerr << std::endl;
    }

    return 0;
}
