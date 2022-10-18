////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022      Antonio Fermiano
// Copyright (C) 2015-2017 Maxime Gauduin
// Copyright (C) 2002-2011 Neill Corlett
//
// This file is part of libecm.
//
// libecm is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libecm is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////

#include "cmdlinecommon.h"

#include "stddef.h"
#include "stdlib.h"
#include "ctype.h"

#if defined(_WIN32)

//
// Detect if the user double-clicked on the .exe rather than executing this from
// the command line, and if so, display a warning and wait for input before
// exiting
//
#include <windows.h>

static HWND getconsolewindow(void) {
    HWND hConsoleWindow = NULL;
    HANDLE k32;
    //
    // See if GetConsoleWindow is available (Windows 2000 or later)
    //
    k32 = LoadLibrary(TEXT("kernel32.dll"));
    if(k32) {
        typedef HWND (* WINAPI gcw_t)(void);
        gcw_t gcw = (gcw_t)GetProcAddress(k32, TEXT("GetConsoleWindow"));
        if(gcw) {
            hConsoleWindow = gcw();
        }
        FreeLibrary(k32);
    }
    //
    // If that didn't work, try the FindWindow trick
    //
    if(!hConsoleWindow) {
        TCHAR savedTitle[1024];
        TCHAR tempTitle[64];
        DWORD id = GetCurrentProcessId();
        unsigned i;
        //
        // Create a random temporary title
        //
        sprintf(tempTitle, "%08lX", (unsigned long)(id));
        srand(id + time(NULL));
        for(i = 8; i < sizeof(tempTitle) - 1; i++) {
            tempTitle[i] = 0x20 + (rand() % 95);
        }
        tempTitle[sizeof(tempTitle) - 1] = 0;
        if(GetConsoleTitle(savedTitle, sizeof(savedTitle))) {
            SetConsoleTitle(tempTitle);
            //
            // Sleep for a tenth of a second to make sure the title actually got set
            //
            Sleep(100);
            //
            // Find the console HWND using the temp title
            //
            hConsoleWindow = FindWindow(0, tempTitle);
            //
            // Restore the old title
            //
            SetConsoleTitle(savedTitle);
        }
    }
    return hConsoleWindow;
}

static void commandlinewarning(void) {
    HWND hConsoleWindow = getconsolewindow();
    DWORD processId = 0;
    //
    // See if the console window belongs to my own process
    //
    if(!hConsoleWindow) { return; }
    GetWindowThreadProcessId(hConsoleWindow, &processId);
    if(GetCurrentProcessId() == processId) {
        printf(
            "\n"
            "Note: This is a command-line application.\n"
            "It was meant to run from a Windows command prompt.\n\n"
            "Press ENTER to close this window..."
        );
        fflush(stdout);
        fgetc(stdin);
    }
}

#else

static void commandlinewarning(void) {}

#endif

static void banner_ok(void) {
    printf(TITLE "\n"
        "  " COPYR "\n"
        "  from Command-Line Pack "
        " (%d-bit "

#if defined(__CYGWIN__)
    "Windows, Cygwin"
#elif defined(__MINGW32__)
    "Windows, MinGW"
#elif defined(_WIN32) && defined(_MSC_VER) && (defined(__alpha) || defined(__ALPHA) || defined(__Alpha_AXP))
    "Windows, Digital AXP C"
#elif defined(_WIN32) && defined(_MSC_VER) && defined(_M_ALPHA)
    "Windows, Microsoft C, Alpha"
#elif defined(_WIN32) && defined(_MSC_VER) && defined(_M_MRX000)
    "Windows, Microsoft C, MIPS"
#elif defined(_WIN32) && defined(_MSC_VER)
    "Windows, Microsoft C"
#elif defined(__WIN32__) || defined(_WIN32)
    "Windows"
#elif defined(__DJGPP__)
    "DOS, DJGPP"
#elif defined(__MSDOS__) && defined(__TURBOC__)
    "DOS, Turbo C"
#elif defined(_DOS) && defined(__WATCOMC__)
    "DOS, Watcom"
#elif defined(__MSDOS__) || defined(MSDOS) || defined(_DOS)
    "DOS"
#elif defined(__APPLE__)
    "Mac OS"
#elif defined(__linux) || defined(__linux__) || defined(__gnu_linux__) || defined(linux)
    "Linux"
#elif defined(__OpenBSD__)
    "OpenBSD"
#elif defined(BSD)
    "BSD"
#elif defined(human68k) || defined(HUMAN68K) || defined(__human68k) || defined(__HUMAN68K) || defined(__human68k__) || defined(__HUMAN68K__)
    "Human68k"
#elif defined(__unix__) || defined(__unix) || defined(unix)
    "unknown Unix"
#else
    "unknown platform"
#endif

        "%s)\n"
        "  http://www.neillcorlett.com/cmdpack/\n"
        "\n",
        (int)(sizeof(size_t) * 8),
        (sizeof(off_t) > 4 && sizeof(off_t) > sizeof(size_t)) ? ", large file support" : ""
    );
}


static void banner_error(void) {
    printf("Configuration error\n");
    exit(1);
}

void banner(void) {
    ((sizeof(off_t) >= sizeof(size_t)) ? banner_ok : banner_error)();
    //
    // If we've displayed the banner, we'll also want to warn that this is a
    // command-line app when we exit
    //
    atexit(commandlinewarning);
}

void normalize_argv0(char* argv0) {
    size_t i;
    size_t start = 0;
    int c;
    for(i = 0; argv0[i]; i++) {
        if(argv0[i] == '/' || argv0[i] == '\\') {
            start = i + 1;
        }
    }
    i = 0;
    do {
        c = ((unsigned char)(argv0[start + i]));
        if(c == '.') { c = 0; }
        if(c != 0) { c = tolower(c); }
        argv0[i++] = c;
    } while(c != 0);
}

static void fprintdec_digit(FILE* f, off_t off) {
    if(off == 0) { return; }
    if(off >= 10) {
        fprintdec_digit(f, off / ((off_t)10));
        off %= ((off_t)10);
    }
    fputc('0' + off, f);
}

void fprintdec(FILE* f, off_t off) {
    if(off == 0) {
        fputc('0', f);
        return;
    }
    if(off < 0) {
        fputc('-', f);
        off = -off;
        if(off < 0) {
            off_t ones = off % ((off_t)10);
            off /= ((off_t)10);
            off = -off;
            fprintdec_digit(f, off);
            fputc('0' - ones, f);
            return;
        }
    }
    fprintdec_digit(f, off);
}
