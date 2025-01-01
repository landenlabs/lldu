//-------------------------------------------------------------------------------------------------
//
// File: filters.hpp  Author: Dennis Lang
// Desc: Filters to further validate a grep match.
//
//-------------------------------------------------------------------------------------------------
//
// Author: Dennis Lang - 2015
// https://landenlabs.com
//
// This file is part of JavaTree project.
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

#pragma once
// ---------------------------------------------------------------------------
class Filter {
public:
    virtual
    void init(const char* pBuffer) {
    }

    virtual
    bool valid(size_t pos, size_t len) {
        return true;
    }
};

// ---------------------------------------------------------------------------
struct Zone {
    Zone(size_t n1, size_t n2) {
        first = n1;
        second = n2;
    }
    size_t first;
    size_t second;
};

// ---------------------------------------------------------------------------
class BufferFilter : public Filter {
public:
    const char* pBuffer;
    std::vector<Zone> zones;
    size_t lastPos = 0;
    size_t lineCnt = 0;
    char eol = '\n';

    virtual
    void init(const char* _pBuffer) {
        pBuffer = _pBuffer;
        lastPos = lineCnt = 0;
    }

    virtual
    bool valid(size_t pos, size_t lenIgnored) {
        countTo(pos);
        return inZones(lineCnt);
    }

    void countTo(size_t pos) {
        const char* cPtr = pBuffer + lastPos;
        pos -= lastPos;
        const char* endPtr = cPtr + pos;
        while (cPtr < endPtr) {
            if (*cPtr++ == eol) {
                lineCnt++;
            }
        }
        lastPos = pos;
    }

    bool inZones(size_t lineNum) const {
        auto it = zones.begin();
        while (it != zones.end()) {
            const Zone& zone = *it;
            if (lineNum >= zone.first && lineNum <= zone.second) {
                return true;
            } else {
                ++it;
            }
        }
        return false;
    }
};

// ---------------------------------------------------------------------------
class LineFilter : public Filter {
public:
    std::vector<Zone> zones;
    size_t lineCnt = 0;
    char eol = '\n';

    virtual
    void init(const char* _pBuffer) {
        lineCnt = 0;
    }

    virtual
    bool valid(size_t pos, size_t lenIgnored) {
        lineCnt++;
        return inZones(lineCnt);
    }

    bool inZones(size_t lineNum) const {
        auto it = zones.begin();
        while (it != zones.end()) {
            const Zone& zone = *it;
            if (lineNum >= zone.first && lineNum <= zone.second) {
                return true;
            } else {
                ++it;
            }
        }
        return false;
    }
};

