//-----------------------------------------------------------------------------
// comma -  Function to enable or disable automatic commas in numeric output.
//
// Author: Dennis Lang - 2015
// http://landenlabs.com/
//
// This file is part of LLFile project.
//
// ----- License ----
//
// Copyright (c) 2015 Dennis Lang
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
//-----------------------------------------------------------------------------


#pragma once

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

