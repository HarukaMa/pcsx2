/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */
#ifndef __PS2ETYPES_H__
#define __PS2ETYPES_H__

#if defined (__linux__) && !defined(__LINUX__)  // some distributions are lower case
#define __LINUX__
#endif

#ifdef __CYGWIN__
#define __LINUX__
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x)/sizeof((x)[0]))
#endif

#ifdef __LINUX__
#define CALLBACK
#else
#define CALLBACK    __stdcall
#endif


// jASSUME - give hints to the optimizer
//  This is primarily useful for the default case switch optimizer, which enables VC to
//  generate more compact switches.

#ifdef NDEBUG
#	define jBREAKPOINT() ((void) 0)
#	ifdef _MSC_VER
#		define jASSUME(exp) (__assume(exp))
#	else
#		define jASSUME(exp) ((void) sizeof(exp))
#	endif
#else
#	if defined(_MSC_VER)
#		define jBREAKPOINT() do { __asm int 3 } while(0)
#	else
#		define jBREAKPOINT() ((void) *(volatile char *) 0)
#	endif
#	define jASSUME(exp) if(exp) ; else jBREAKPOINT()
#endif

// disable the default case in a switch
#define jNO_DEFAULT \
{ \
	break; \
	\
default: \
	jASSUME(0); \
	break; \
}


// Basic types
#if defined(_MSC_VER)

typedef __int8  s8;
typedef __int16 s16;
typedef __int32 s32;
typedef __int64 s64;

typedef unsigned __int8  u8;
typedef unsigned __int16 u16;
typedef unsigned __int32 u32;
typedef unsigned __int64 u64;

typedef unsigned int uint;

#define PCSX2_ALIGNED(alig,x) __declspec(align(alig)) x
#define PCSX2_ALIGNED16(x) __declspec(align(16)) x
#define PCSX2_ALIGNED16_DECL(x) __declspec(align(16)) x

#else

#ifdef __LINUX__

#ifdef HAVE_STDINT_H
#include "stdint.h"

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uintptr_t uptr;
typedef intptr_t sptr;

#else

typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

#endif

typedef unsigned int uint;

#define LONG long
typedef union _LARGE_INTEGER
{
	long long QuadPart;
} LARGE_INTEGER;

#define __fastcall __attribute__((fastcall))
#define __unused __attribute__((unused))
#define _inline __inline__ __attribute__((unused))
#define __forceinline __attribute__((always_inline,unused))
#endif

#if defined(__MINGW32__)
#define PCSX2_ALIGNED(alig,x) __declspec(align(alig)) x
#define PCSX2_ALIGNED16(x) __declspec(align(16)) x
#else
#define PCSX2_ALIGNED(alig,x) x __attribute((aligned(alig)))
#define PCSX2_ALIGNED16(x) x __attribute((aligned(16)))
#endif

#define PCSX2_ALIGNED16_DECL(x) x


#endif // _MSC_VER

#if !defined(__LINUX__) || !defined(HAVE_STDINT_H)
#if defined(__x86_64__)
typedef u64 uptr;
typedef s64 sptr;
#else
typedef u32 uptr;
typedef s32 sptr;
#endif
#endif

typedef struct {
	int size;
	s8 *data;
} freezeData;

/* common defines */
#ifndef C_ASSERT
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
#endif

#endif /* __PS2ETYPES_H__ */