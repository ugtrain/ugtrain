/* memattach.h:    functions to attach/read/write victim proc. memory
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

#ifndef MEMATTACH_H
#define MEMATTACH_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif
	i32 memattach_test (pid_t pid);
	i32 memattach (pid_t pid);
	i32 memdetach (pid_t pid);
	i32 memread   (pid_t pid, void *addr, void *buf, long buf_len);
	i32 memwrite  (pid_t pid, void *addr, void *buf, long buf_len);
#ifdef __cplusplus
};
#endif

#endif
