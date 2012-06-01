/***************************************************************************
 *
 *  A Game Cheater Library
 *  Copyright (C) 2005 Alf <h980501427@hotmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 **************************************************************************/

#include "libgcheater.h"

int gc_ptrace_test(pid_t pid)
{
  if (pid <= 1) return -1;

  errno = 0;
  ptrace(PTRACE_ATTACH, pid, 0, 0);

  waitpid(pid, NULL, 0); // wait child process stop

  ptrace(PTRACE_DETACH, pid, 0, SIGCONT);

  if (errno != 0) {
    errno = 0;
    return -1;
  } else {
    return 0;
  }

  return -1;
}

int gc_ptrace_stop(pid_t pid)
{
  if (pid <= 1) return -1;

  errno = 0;
  ptrace(PTRACE_ATTACH, pid, 0, 0);

  waitpid(pid, NULL, 0);

  if (errno != 0) {
    ptrace(PTRACE_DETACH, pid, 0, SIGCONT);
    errno = 0;
    return -1;
  } else {
    return 0;
  }

  return -1;
}

int gc_ptrace_continue(pid_t pid)
{
  if (pid <= 1) return -1;

  errno = 0;
  ptrace(PTRACE_DETACH, pid, 0, SIGCONT);

  if (errno != 0) {
    errno = 0;
    return -1;
  } else {
    return 0;
  }

  return -1;
}

int gc_get_memory(pid_t pid, void* addr, void* buf, long buf_len)
{
  if (pid <= 1 || addr == NULL || buf == NULL || buf_len <= 0) return -1;

  long x = 0;

  int i = 0;
  int j = buf_len / sizeof(long);
  for (i = 0; i < j; i++) {
    x = ptrace(PTRACE_PEEKDATA, pid, addr + i * sizeof(long), x);
    if (errno != 0) goto error;
    memcpy(buf + i * sizeof(long), &x, sizeof(long));
  }

  j = buf_len % sizeof(long);
  if (j > 0) {
    x = ptrace(PTRACE_PEEKDATA, pid, addr + i * sizeof(long), x);
    if (errno != 0) goto error;
    memcpy(buf + i * sizeof(long), &x, j);
  }

  return 0;

error:

  return -1;
}

int gc_set_memory(pid_t pid, void* addr, void* buf, long buf_len)
{
  if (pid <= 1 || addr == NULL || buf == NULL || buf_len <= 0) return -1;

  long x = 0;

  int i = 0;
  int j = buf_len / sizeof(long);
  for (i = 0; i < j; i++) {
    memcpy(&x, buf + i*sizeof(long), sizeof(long));
    ptrace(PTRACE_POKEDATA, pid, addr + i*sizeof(long), x);
    if (errno != 0) goto error;
  }

  j = buf_len % sizeof(long);
  if (j > 0) {
    x = ptrace(PTRACE_PEEKDATA, pid, addr, x);
    if (errno != 0) goto error;
    memcpy(&x, buf + i*sizeof(long), j);
    ptrace(PTRACE_POKEDATA, pid, addr + i*sizeof(long), x);
    if (errno != 0) goto error;
  }

  return 0;

error:

  return -1;
}
