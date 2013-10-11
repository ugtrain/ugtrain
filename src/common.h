/* common.h:    common C/C++ helpers
 *
 * Copyright (c) 2013, by:      Sebastian Riemer
 *    All rights reserved.     <sebastian.riemer@gmx.de>
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

// Common types
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
typedef ulong ptr_t;

// Common defines
#define MAX_STACK 8

// for Windows as not in limits.h
#ifndef PIPE_BUF
	#define PIPE_BUF 4096
#endif

// Common macros
#define PTR_ADD(type, x, y)  (type) ((ptr_t)x + (ptr_t)y)
#define PTR_ADD2(type, x, y, z)  (type) ((ptr_t)x + (ptr_t)y + (ptr_t)z)
#define PTR_SUB(type, x, y)  (type) ((ptr_t)x - (ptr_t)y)

// Common functions
#ifdef __cplusplus
	template <class T>
	string to_string (T val);
	char *to_c_str(string *str);
#endif

#endif
