/* memattach.h:    functions to attach/read/write victim proc. memory
 *
 * Copyright (c) 2013, by:      Sebastian Riemer
 *    All rights reserved.      Ernst-Reinke-Str. 23
 *                              10369 Berlin, Germany
 *                             <sebastian.riemer@gmx.de>
 *
 * based on libgcheater by Alf <h980501427@hotmail.com>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 */

#ifndef MEMATTACH_H
#define MEMATTACH_H

#ifdef __cplusplus
extern "C" {
#endif
	int memattach_test (pid_t pid);
	int memattach (pid_t pid);
	int memdetach (pid_t pid);
	int memread   (pid_t pid, void* addr, void* buf, long buf_len);
	int memwrite  (pid_t pid, void* addr, void* buf, long buf_len);
#ifdef __cplusplus
};
#endif

#endif
