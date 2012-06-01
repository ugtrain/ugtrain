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

#ifndef _LIBGCHEATER_H_
#define _LIBGCHEATER_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

int gc_ptrace_test(pid_t pid);

int gc_ptrace_stop(pid_t pid);

int gc_ptrace_continue(pid_t pid);

int gc_get_memory(pid_t pid, void* addr, void* buf, long buf_len);

int gc_set_memory(pid_t pid, void* addr, void* buf, long buf_len);

#ifdef __cplusplus
}
#endif

#endif
