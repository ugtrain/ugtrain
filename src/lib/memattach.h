/* memattach.h:    functions to attach/read/write victim proc. memory
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
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
 *
 * By the original authors of ugtrain there shall be ABSOLUTELY
 * NO RESPONSIBILITY or LIABILITY for derived work and/or abusive
 * or malicious use. The ugtrain is an education project and
 * sticks to the copyright law by providing configs for games
 * which ALLOW CHEATING only. We can't and we don't accept abusive
 * configs or codes which might turn ugtrain into a cracker tool!
 */

#ifndef MEMATTACH_H
#define MEMATTACH_H

/* local includes */
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif
	i32 memattach_test (pid_t pid);
	i32 memattach (pid_t pid);
	i32 memdetach (pid_t pid);
	i32 memread   (pid_t pid, ptr_t addr, void *buf, size_t buf_len);
	i32 memwrite  (pid_t pid, ptr_t addr, void *buf, size_t buf_len);
#ifdef __cplusplus
};
#endif

#endif
