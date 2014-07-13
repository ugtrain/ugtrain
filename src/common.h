/* common.h:    common C/C++ helpers
 *
 * Copyright (c) 2012..14, by:  Sebastian Parschauer
 *    All rights reserved.     <s.parschauer@gmx.de>
 *
 * powered by the Open Game Cheating Association
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef COMMON_H
#define COMMON_H

#include <limits.h>
#ifdef __cplusplus
	#include <iostream>
	#include <string>
	using namespace std;
#else
	#include <stdbool.h>
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
#ifndef __linux__
	typedef int pid_t;
#endif

/* uintptr_t is optional */
#if !defined(HAVE_UINTPTR_T) || !HAVE_UINTPTR_T
#undef SCNxPTR
#undef PRIxPTR
#undef UINTPTR_MAX
#if defined(_WIN64) || defined (__WIN64__)
/* 64 bit Windows systems require unsigned long long */
#define uintptr_t u64
#define SCN_PTR "0x%llx"
#define PRI_PTR "0x%llx"
#define UINTPTR_MAX ULLONG_MAX
#else
/* unsigned long usable */
#define uintptr_t ulong
#define SCN_PTR "0x%lx"
#define PRI_PTR "0x%lx"
#define UINTPTR_MAX ULONG_MAX
#endif
#else
#include <stdint.h>
#include <inttypes.h>
#define SCN_PTR "0x%"SCNxPTR
#define PRI_PTR "0x%"PRIxPTR
#endif
typedef uintptr_t ptr_t;

#if defined(_WIN64) || defined (__WIN64__)
#define strtoptr strtoull
#else
#define strtoptr strtoul
#endif

/* Common defines */
#define GBT_CMD     "gbt"  /* GNU backtrace() activation cmd */
#define MAX_GNUBT   4      /* for GNU backtrace() */
#define PRELOAD_VAR "LD_PRELOAD"
#define UGT_GAME_PROC_NAME     "UGT_GAME_PROC_NAME"

/* for Windows as not in limits.h */
#ifndef PIPE_BUF
	#define PIPE_BUF 4096
#endif

/* Common macros */
#define PTR_ADD(type, x, y)  (type) ((ptr_t)x + (ptr_t)y)
#define PTR_ADD2(type, x, y, z)  (type) ((ptr_t)x + (ptr_t)y + (ptr_t)z)
#define PTR_SUB(type, x, y)  (type) ((ptr_t)x - (ptr_t)y)

#define list_for_each(list, it) \
	for (it = list->begin(); it != list->end(); ++it)
#define vect_for_each(vect, it) \
	list_for_each(vect, it)

/* Common functions */
#ifdef __cplusplus
	template <class T>
	string to_string (T val);
	template <class T>
	string to_xstring (T val);
	char *to_c_str(string *str);
#endif

#endif
