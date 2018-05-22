/* memattach_l.h:    functions to attach/read/write victim proc. memory
 * This file is for Linux only.
 *
 * Copyright (c) 2012..2018 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * inspired by libgcheater by Alf <h980501427@hotmail.com>
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

#ifdef __linux__
#ifndef MEMATTACH_L_H
#define MEMATTACH_L_H

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <errno.h>


static inline
i32 memattach_test (pid_t pid, i32 *fd)
{
	if (pid <= 1)
		goto err;

	errno = 0;
	ptrace(PTRACE_ATTACH, pid, 0, 0);
	if (errno != 0)
		goto err;

	waitpid(pid, NULL, 0);  /* wait for victim process stop */
#ifdef HAVE_PROCMEM
	char path[32];

	snprintf(path, sizeof(path), "/proc/%d/mem", pid);
	*fd = open(path, O_RDWR);
	if (*fd < 0) {
		ptrace(PTRACE_DETACH, pid, 0, SIGCONT);
		goto err;
	}
#endif
	ptrace(PTRACE_DETACH, pid, 0, SIGCONT);
	if (errno != 0)
		goto err;

	return 0;
err:
	return -1;
}

static inline
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

static inline
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

static inline
i32 memread (pid_t pid, ptr_t addr, void *_buf, size_t buf_len)
{
	char *buf = (char *) _buf;

	if (pid <= 1 || addr == 0 || buf == NULL || buf_len <= 0)
		return -1;

#ifdef HAVE_PROCMEM
	i32 fd = (i32) pid;
	/* TODO: Handle unlikely incomplete reads properly */
	if (pread(fd, buf, buf_len, addr) < (ssize_t) buf_len)
		goto err;
#else
	ptr_t read_val = 0;
	size_t pos = 0;      /* position in sizeof(ptr_t) steps */
	size_t len = buf_len / sizeof(ptr_t);

	errno = 0;
	for (pos = 0; pos < len; pos++) {
		read_val = ptrace(PTRACE_PEEKDATA, pid,
			addr + pos * sizeof(ptr_t), read_val);
		if (errno != 0)
			goto err;
		memcpy(&buf[pos * sizeof(ptr_t)], &read_val, sizeof(ptr_t));
	}

	/* remainder processing */
	len = buf_len % sizeof(ptr_t);
	if (len > 0) {
		read_val = ptrace(PTRACE_PEEKDATA, pid,
			addr + pos * sizeof(ptr_t), read_val);
		if (errno != 0)
			goto err;
		memcpy(&buf[pos * sizeof(ptr_t)], &read_val, len);
	}
#endif
	return 0;
err:
	return -1;
}

static inline
i32 memwrite (pid_t pid, ptr_t addr, void *_buf, size_t buf_len)
{
	char *buf = (char *) _buf;

	if (pid <= 1 || addr == 0 || buf == NULL || buf_len <= 0)
		return -1;

#ifdef HAVE_PROCMEM
	i32 fd = (i32) pid;
	/* TODO: Handle unlikely incomplete writes properly */
	if (pwrite(fd, buf, buf_len, addr) < (ssize_t) buf_len)
		goto err;
#else
	ptr_t rw_val = 0;
	size_t pos = 0;      /* position in sizeof(ptr_t) steps */
	size_t len = buf_len / sizeof(ptr_t);

	errno = 0;
	for (pos = 0; pos < len; pos++) {
		memcpy(&rw_val, &buf[pos * sizeof(ptr_t)], sizeof(ptr_t));
		ptrace(PTRACE_POKEDATA, pid,
			addr + pos * sizeof(ptr_t), rw_val);
		if (errno != 0)
			goto err;
	}

	/* remainder processing */
	len = buf_len % sizeof(ptr_t);
	if (len > 0) {
		rw_val = ptrace(PTRACE_PEEKDATA, pid, addr, rw_val);
		if (errno != 0)
			goto err;
		memcpy(&rw_val, &buf[pos * sizeof(ptr_t)], len);
		ptrace(PTRACE_POKEDATA, pid,
			addr + pos * sizeof(ptr_t), rw_val);
		if (errno != 0)
			goto err;
	}
#endif
	return 0;
err:
	return -1;
}

#endif /* MEMATTACH_L_H */
#endif
