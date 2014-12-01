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

#ifdef __cplusplus
	#include <iostream>
	#include <string>
	using namespace std;
#endif

/* local includes */
#include <lib/types.h>

/* Common defines */
#define GBT_CMD     "gbt"  /* GNU backtrace() activation cmd */
#define MAX_GNUBT   4      /* for GNU backtrace() */
#define UGT_GAME_PROC_NAME     "UGT_GAME_PROC_NAME"
#define DBG_FILE_NAME "/tmp/lib_dbg.txt"

/* for Windows as not in limits.h */
#ifndef PIPE_BUF
	#define PIPE_BUF 4096
#endif

/* Common macros */
#define PTR_ADD(type, x, y)  (type) ((ptr_t)x + (ptr_t)y)
#define PTR_ADD2(type, x, y, z)  (type) ((ptr_t)x + (ptr_t)y + (ptr_t)z)
#define PTR_SUB(type, x, y)  (type) ((ptr_t)x - (ptr_t)y)

/* common helpers e.g. for lists taken from Linux kernel */
#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

/* C++ list/vector processing */
#ifdef __cplusplus
#define list_for_each(list, it) \
	for (it = list->begin(); it != list->end(); ++it)
#define vect_for_each(vect, it) \
	list_for_each(vect, it)
#endif

/* Common functions */
#ifdef __cplusplus
	template <class T>
	string to_string (T val);
	template <class T>
	string to_xstring (T val);
	char *to_c_str(string *str);
#endif

#endif
