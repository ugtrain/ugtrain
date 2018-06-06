/* common.h:    common C/C++ helpers
 *
 * Copyright (c) 2012..2018 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
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

/* common types */
enum grow_type {
	GROW_ADD
};

/* Common defines */
#define GBT_CMD     "gbt"  /* GNU backtrace() activation cmd */
#define MAX_GNUBT   4      /* for GNU backtrace() */
#define UGT_GAME_PROC_NAME     "UGT_GAME_PROC_NAME"
#define DBG_FILE_NAME          "/tmp/lib_dbg.txt"
#define ASLR_NOTIFY_STR        "ready\n"
#define ugout                  cout << "[ugt] "
#define ugerr                  cerr << "[ugt] "

/* Branch optimization */
#define likely(x)     __builtin_expect(!!(x), 1)
#define unlikely(x)   __builtin_expect(!!(x), 0)

/* Common macros */
#define PTR_ADD(type, x, y)  (type) ((ptr_t)x + (ptr_t)y)
#define PTR_ADD2(type, x, y, z)  (type) ((ptr_t)x + (ptr_t)y + (ptr_t)z)
#define PTR_SUB(type, x, y)  (type) ((ptr_t)x - (ptr_t)y)

/* C++ list/vector processing */
#ifdef __cplusplus
#define list_for_each(list, it) \
	for (it = list->begin(); it != list->end(); ++it)
#define vect_for_each(vect, it) \
	list_for_each(vect, it)
/* Iterate over a list backwards */
#define list_for_each_prev(list, rit) \
	for (rit = list->rbegin(); rit != list->rend(); ++rit)
#define vect_for_each_prev(vect, rit) \
	list_for_each_prev(vect, rit)
#endif

/* Common functions */
#ifdef __cplusplus
	char *to_c_str(string *str);
	char *to_c_str_c(string *str);
#endif

#endif
