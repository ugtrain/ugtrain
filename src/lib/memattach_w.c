/* memattach_w.c:    functions to attach/read/write victim proc. memory
 * This file is for Windows only.
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
 *
 * By the original authors of ugtrain there shall be ABSOLUTELY
 * NO RESPONSIBILITY or LIABILITY for derived work and/or abusive
 * or malicious use. The ugtrain is an education project and
 * sticks to the copyright law by providing configs for games
 * which ALLOW CHEATING only. We can't and we don't accept abusive
 * configs or codes which might turn ugtrain into a cracker tool!
 */

#if defined(__WINNT__) || defined (__WIN32__)

/* local includes */
#include "memattach.h"


i32 memattach_test (pid_t pid)
{
	return -1;
}

i32 memattach (pid_t pid)
{
	return -1;
}

i32 memdetach (pid_t pid)
{
	return -1;
}

i32 memread (pid_t pid, ptr_t addr, void *buf, ulong buf_len)
{
	return -1;
}

i32 memwrite (pid_t pid, ptr_t addr, void *buf, ulong buf_len)
{
	return -1;
}

#endif
