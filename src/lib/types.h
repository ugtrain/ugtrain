/* types.h:    common multi-platform data types
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * powered by the Open Game Cheating Association
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef TYPES_H
#define TYPES_H

#include <limits.h>
#include <stddef.h>  /* size_t */
#if defined(HAVE_UNISTD_H) && HAVE_UNISTD_H
	#include <unistd.h>  /* pid_t, ssize_t */
#endif
#ifndef __cplusplus
	#include <stdbool.h>  /* bool */
#endif

/* Common types */
typedef char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef long long i64;
typedef unsigned long long u64;
typedef unsigned long ulong;

/* uintptr_t is optional */
#if !defined(HAVE_UINTPTR_T) || !HAVE_UINTPTR_T
	#undef uintptr_t
	#if defined(_WIN64) || defined (__WIN64__)
		/* 64-bit Windows systems require unsigned long long */
		#define uintptr_t u64
	#elif ULONG_MAX == 4294967295UL
		#define uintptr_t u32
	#else
		#define uintptr_t ulong
	#endif
#else
	#include <stdint.h>
	#include <inttypes.h>
#endif
/* These formats are not available with default
   C++ on MeeGo 1.2 Harmattan. */
#if !defined(SCNxPTR) || !defined(PRIxPTR) || !defined(UINTPTR_MAX)
	#undef SCNxPTR
	#undef PRIxPTR
	#undef UINTPTR_MAX
	#if defined(_WIN64) || defined (__WIN64__)
		#define SCNxPTR "llx"
		#define PRIxPTR "llx"
		#define UINTPTR_MAX ULLONG_MAX
	#elif ULONG_MAX == 4294967295UL
		#define SCNxPTR "x"
		#define PRIxPTR "x"
		#define UINTPTR_MAX ULONG_MAX
	#else
		#define SCNxPTR "lx"
		#define PRIxPTR "lx"
		#define UINTPTR_MAX ULONG_MAX
	#endif
#endif
#define SCN_PTR "0x%" SCNxPTR
#define PRI_PTR "0x%" PRIxPTR
typedef uintptr_t ptr_t;

#if defined(_WIN64) || defined (__WIN64__)
	#define strtoptr strtoull
#else
	#define strtoptr strtoul
#endif

#endif
