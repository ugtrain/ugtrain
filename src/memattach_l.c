/* memattach_l.c:    functions to attach/read/write victim proc. memory
 * This file is for Linux only.
 *
 * Copyright (c) 2012..14, by:  Sebastian Parschauer
 *    All rights reserved.     <s.parschauer@gmx.de>
 *
 * powered by the Open Game Cheating Association
 * based on libgcheater by Alf <h980501427@hotmail.com>
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

#ifdef __linux__

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <errno.h>
#include "memattach.h"


i32 memattach_test (pid_t pid)
{
	if (pid <= 1)
		goto err;

	errno = 0;
	ptrace(PTRACE_ATTACH, pid, 0, 0);
	if (errno != 0)
		goto err;

	waitpid(pid, NULL, 0);  /* wait for victim process stop */

	ptrace(PTRACE_DETACH, pid, 0, SIGCONT);
	if (errno != 0)
		goto err;

	return 0;
err:
	return -1;
}

i32 memattach (pid_t pid)
{
	if (pid <= 1)
		goto err;

	errno = 0;
	ptrace(PTRACE_ATTACH, pid, 0, 0);
	if (errno != 0)
		goto err;

	waitpid(pid, NULL, 0);
	if (errno != 0) {
		ptrace(PTRACE_DETACH, pid, 0, SIGCONT);
		goto err;
	}
	return 0;
err:
	return -1;
}

i32 memdetach (pid_t pid)
{
	if (pid <= 1)
		goto err;

	errno = 0;
	ptrace(PTRACE_DETACH, pid, 0, SIGCONT);
	if (errno != 0)
		goto err;

	return 0;
err:
	return -1;
}

i32 memread (pid_t pid, void *addr, void *buf, long buf_len)
{
	long read_val = 0;
	long pos = 0;      /* position in sizeof(long) steps */
	long len = buf_len / sizeof(long);

	if (pid <= 1 || addr == NULL || buf == NULL || buf_len <= 0)
		return -1;

	errno = 0;
	for (pos = 0; pos < len; pos++) {
		read_val = ptrace(PTRACE_PEEKDATA, pid,
			addr + pos * sizeof(long), read_val);
		if (errno != 0)
			goto err;
		memcpy(buf + pos * sizeof(long), &read_val, sizeof(long));
	}

	/* remainder processing */
	len = buf_len % sizeof(long);
	if (len > 0) {
		read_val = ptrace(PTRACE_PEEKDATA, pid,
			addr + pos * sizeof(long), read_val);
		if (errno != 0)
			goto err;
		memcpy(buf + pos * sizeof(long), &read_val, len);
	}

	return 0;
err:
	return -1;
}

i32 memwrite (pid_t pid, void *addr, void *buf, long buf_len)
{
	long rw_val = 0;
	long pos = 0;      /* position in sizeof(long) steps */
	long len = buf_len / sizeof(long);

	if (pid <= 1 || addr == NULL || buf == NULL || buf_len <= 0)
		return -1;

	errno = 0;
	for (pos = 0; pos < len; pos++) {
		memcpy(&rw_val, buf + pos * sizeof(long), sizeof(long));
		ptrace(PTRACE_POKEDATA, pid,
			addr + pos * sizeof(long), rw_val);
		if (errno != 0)
			goto err;
	}

	/* remainder processing */
	len = buf_len % sizeof(long);
	if (len > 0) {
		rw_val = ptrace(PTRACE_PEEKDATA, pid, addr, rw_val);
		if (errno != 0)
			goto err;
		memcpy(&rw_val, buf + pos * sizeof(long), len);
		ptrace(PTRACE_POKEDATA, pid,
			addr + pos * sizeof(long), rw_val);
		if (errno != 0)
			goto err;
	}

	return 0;
err:
	return -1;
}

#endif
