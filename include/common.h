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

#pragma once

// Disable fopen() warnings on VC++. It means well...
#define _CRT_SECURE_NO_WARNINGS

// Try to enable 64-bit file offsets on platforms where it's optional
#define _LARGEFILE64_SOURCE 1
#define __USE_FILE_OFFSET64 1
#define __USE_LARGEFILE64 1
#define _FILE_OFFSET_BITS 64

// Try to enable long filename support on Watcom
#define __WATCOM_LFN__ 1

// Convince MinGW that we want to glob arguments
#ifdef __MINGW32__
int _dowildcard = -1;
#endif

////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

// Try to bring in unistd.h if possible
#if !defined(__TURBOC__) && !defined(_MSC_VER)
#include <unistd.h>
#endif

// Bring in direct.h if we need to; sometimes mkdir/rmdir is defined here
#if defined(__WATCOMC__) || defined(_MSC_VER)
#include <direct.h>
#endif

// Fill in S_ISDIR
#if !defined(_POSIX_VERSION) && !defined(S_ISDIR)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#if defined(__TURBOC__) || defined(__WATCOMC__) || defined(__MINGW32__) || defined(_MSC_VER)
//
// Already have a single-argument mkdir()
//
#else
//
// Provide a single-argument mkdir()
//
#define mkdir(a) mkdir(a, S_IRWXU | S_IRWXG | S_IRWXO)
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Enforce large memory model for 16-bit DOS targets
//
#if defined(__MSDOS__) || defined(MSDOS)
#if defined(__TURBOC__) || defined(__WATCOMC__)
#if !defined(__LARGE__)
#error This is not the memory model we should be using!
#endif
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Try to figure out integer types
//
#if defined(_STDINT_H) || defined(_EXACT_WIDTH_INTS)

// _STDINT_H_ - presume stdint.h has already been included
// _EXACT_WIDTH_INTS - OpenWatcom already provides int*_t in sys/types.h

#elif defined(__STDC__) && __STDC__ && __STDC_VERSION__ >= 199901L

// Assume C99 compliance when the compiler specifically tells us it is
#include <stdint.h>

#elif defined(_MSC_VER)

// On Visual Studio, use its integral types
typedef   signed __int8   int8_t;
typedef unsigned __int8  uint8_t;
typedef   signed __int16  int16_t;
typedef unsigned __int16 uint16_t;
typedef   signed __int32  int32_t;
typedef unsigned __int32 uint32_t;

#else

// Guess integer sizes from limits.h

//
// int8_t
//
#ifndef __int8_t_defined
#if SCHAR_MIN == -128 && SCHAR_MAX == 127 && UCHAR_MAX == 255
typedef signed char int8_t;
#else
#error Unknown how to define int8_t!
#endif
#endif

//
// uint8_t
//
#ifndef __uint8_t_defined
#if SCHAR_MIN == -128 && SCHAR_MAX == 127 && UCHAR_MAX == 255
typedef unsigned char uint8_t;
#else
#error Unknown how to define uint8_t!
#endif
#endif

//
// int16_t
//
#ifndef __int16_t_defined
#if SHRT_MIN == -32768 && SHRT_MAX == 32767 && USHRT_MAX == 65535
typedef signed short int16_t;
#else
#error Unknown how to define int16_t!
#endif
#endif

//
// uint16_t
//
#ifndef __uint16_t_defined
#if SHRT_MIN == -32768 && SHRT_MAX == 32767 && USHRT_MAX == 65535
typedef unsigned short uint16_t;
#else
#error Unknown how to define uint16_t!
#endif
#endif

//
// int32_t
//
#ifndef __int32_t_defined
#if    INT_MIN == -2147483648 &&  INT_MAX == 2147483647 &&  UINT_MAX == 4294967295
typedef signed int int32_t;
#elif LONG_MIN == -2147483648 && LONG_MAX == 2147483647 && ULONG_MAX == 4294967295
typedef signed long int32_t;
#else
#error Unknown how to define int32_t!
#endif
#endif

//
// uint32_t
//
#ifndef __uint32_t_defined
#if    INT_MIN == -2147483648 &&  INT_MAX == 2147483647 &&  UINT_MAX == 4294967295
typedef unsigned int uint32_t;
#elif LONG_MIN == -2147483648 && LONG_MAX == 2147483647 && ULONG_MAX == 4294967295
typedef unsigned long uint32_t;
#else
#error Unknown how to define uint32_t!
#endif
#endif

#endif

//
// There are some places in the code where it's assumed 'long' can hold at least
// 32 bits.  Verify that here:
//
#if LONG_MAX < 2147483647 || ULONG_MAX < 4294967295
#error long type must be at least 32 bits!
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Figure out how big file offsets should be
//
#if defined(_OFF64_T_) || defined(_OFF64_T_DEFINED) || defined(__off64_t_defined)
//
// We have off64_t
// Regular off_t may be smaller, so check this first
//

#ifdef off_t
#undef off_t
#endif
#ifdef fseeko
#undef fseeko
#endif
#ifdef ftello
#undef ftello
#endif

#define off_t off64_t
#define fseeko fseeko64
#define ftello ftello64

#elif defined(_OFF_T) || defined(__OFF_T_TYPE) || defined(__off_t_defined) || defined(_OFF_T_DEFINED_)
//
// We have off_t
//

#else
//
// Assume offsets are just 'long'
//
#ifdef off_t
#undef off_t
#endif
#ifdef fseeko
#undef fseeko
#endif
#ifdef ftello
#undef ftello
#endif

#define off_t long
#define fseeko fseek
#define ftello ftell

#endif

////////////////////////////////////////////////////////////////////////////////
//
// Define truncate() for systems that don't have it
//
#if !defined(_POSIX_VERSION)

#if (defined(__MSDOS__) || defined(MSDOS)) && (defined(__TURBOC__) || defined(__WATCOMC__))

#include <dos.h>
#include <io.h>
#include <fcntl.h>
int truncate(const char *filename, off_t size) {
    if(size < 0) {
        errno = EINVAL;
        return -1;
    }
    //
    // Extend (or do nothing) if necessary
    //
    {   off_t end;
        FILE* f = fopen(filename, "rb");
        if(!f) {
            return -1;
        }
        if(fseeko(f, 0, SEEK_END) != 0) {
            fclose(f);
            return -1;
        }
        end = ftello(f);
        if(end <= size) {
            for(; end < size; end++) {
                if(fputc(0, f) == EOF) {
                    fclose(f);
                    return -1;
                }
            }
            fclose(f);
            return 0;
        }
        fclose(f);
    }
    //
    // Shrink if necessary (DOS-specific call)
    //
    {   int doshandle = 0;
        unsigned nwritten = 0;
        if(_dos_open(filename, O_WRONLY, &doshandle)) {
            return -1;
        }
        if(lseek(doshandle, size, SEEK_SET) == -1L) {
            _dos_close(doshandle);
            return -1;
        }
        if(_dos_write(doshandle, &doshandle, 0, &nwritten)) {
            _dos_close(doshandle);
            return -1;
        }
        _dos_close(doshandle);
    }
    //
    // Success
    //
    return 0;
}

#elif (defined(_WIN32) && defined(_MSC_VER))

#if defined(_MSC_VER)
// Disable extension warnings for <windows.h> and friends
#pragma warning (disable: 4226)
#endif

#include <windows.h>

#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ((DWORD)(-1))
#endif

int truncate(const char *filename, off_t size) {
    if(size < 0) {
        errno = EINVAL;
        return -1;
    }
    //
    // Extend (or do nothing) if necessary
    //
    {   off_t end;
        FILE* f = fopen(filename, "rb");
        if(!f) {
            return -1;
        }
        if(fseeko(f, 0, SEEK_END) != 0) {
            fclose(f);
            return -1;
        }
        end = ftello(f);
        if(end <= size) {
            for(; end < size; end++) {
                if(fputc(0, f) == EOF) {
                    fclose(f);
                    return -1;
                }
            }
            fclose(f);
            return 0;
        }
        fclose(f);
    }
    //
    // Shrink if necessary (Windows-specific call)
    //
    {   HANDLE f = CreateFile(
            filename,
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if(f == INVALID_HANDLE_VALUE) {
            return -1;
        }
        if(size > ((off_t)0x7FFFFFFFL)) {
            // use fancy 64-bit SetFilePointer
            LONG lo = size;
            LONG hi = size >> 32;
            if(SetFilePointer(f, lo, &hi, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
                CloseHandle(f);
                return -1;
            }
        } else {
            // use plain 32-bit SetFilePointer
            if(SetFilePointer(f, size, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
                CloseHandle(f);
                return -1;
            }
        }
        if(!SetEndOfFile(f)) {
            CloseHandle(f);
            return -1;
        }
    }
    //
    // Success
    //
    return 0;
}

#endif

#endif // !defined(_POSIX_VERSION)

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// Work around some problems with the Mariko CC toolchain
//
#ifdef MARIKO_CC

// 32-bit signed and unsigned mod seem buggy; this solves it
unsigned long __umodsi3(unsigned long a, unsigned long b) { return a - (a / b) * b; }
signed long __modsi3(signed long a, signed long b) { return a - (a / b) * b; }

// Some kind of soft float linkage issue?
void __cmpdf2(void) {}

#endif

////////////////////////////////////////////////////////////////////////////////

uint32_t get32lsb(const uint8_t* src);
void put32lsb(uint8_t* dest, uint32_t value);
