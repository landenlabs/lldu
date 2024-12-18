//-------------------------------------------------------------------------------------------------
// File: signals.cpp
// Author: Dennis Lang
//
// Desc: Catch system signals (aka Control-c)
//
//-------------------------------------------------------------------------------------------------
//
// Author: Dennis Lang - 2024
// https://landenlabs.com
//
// This file is part of llreplace project.
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

#include "signals.hpp"

#include <iostream>

#ifdef HAVE_WIN
#define byte win_byte_override  // Fix for c++ v17
#include <windows.h>
#undef byte                     // Fix for c++ v17
#else
#include <signal.h>
#endif


volatile bool Signals::aborted = false;    // Set true by signal handler
volatile unsigned int Signals::abortCnt = 0;


#ifdef HAVE_WIN
//-------------------------------------------------------------------------------------------------
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
    case CTRL_C_EVENT:  // Handle the CTRL-C signal.
        Signals::aborted = true;
        std::cerr << "\nCaught signal " << std::endl;
        Beep(750, 300);
        if (Signals::abortCnt++ >= 2)
            exit(-1);
        return TRUE;
    }

    return FALSE;
}

//-------------------------------------------------------------------------------------------------
void Signals::init() {
    if (! SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
        std::cerr << "Failed to install sig handler \a" << endl;
    }
}

#else

//-------------------------------------------------------------------------------------------------
void sigHandler(int /* sig_t */ s) {
    Signals::aborted = true;
    std::cerr << "\nCaught signal \a " << std::endl;
    if (Signals::abortCnt++ >= 2)
        exit(-1);
}

//-------------------------------------------------------------------------------------------------
void Signals::init() {
    // signal(SIGINT, sigHandler);

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = sigHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    if (sigaction(SIGINT, &sigIntHandler, NULL) != 0) {
        std::cerr << "Failed to install sig handler" << endl;
    }
}

#endif
