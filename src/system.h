/* system.h:    provide OS specific helpers (e.g. run cmds)
 *
 * Copyright (c) 2012..13, by:  Sebastian Riemer
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

#ifndef SYSTEM_H
#define SYSTEM_H

#include <unistd.h>
#include "common.h"
#ifndef __linux__
#include <windows.h>
#endif

#ifdef __linux__
static inline u32 sleep_sec (u32 sec)
{
	return sleep(sec);
}
#else
static inline void sleep_sec (u32 sec)
{
	Sleep(sec * 1000);
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
	pid_t   proc_to_pid (char *proc_name);
	i32     run_cmd_bg (const char *cmd, char *const cmdv[],
			    u8 do_wait);
	ssize_t run_cmd_pipe (const char *cmd, char *const cmdv[],
			      char *pbuf, size_t pbuf_size, u8 use_shell);
	i32     fork_wait_kill (pid_t wpid, void (*task) (void *), void *argp);
#ifdef __cplusplus
};
#endif

#endif
