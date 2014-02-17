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


#ifdef __cplusplus
extern "C" {
#endif
	char    *get_abs_app_path (char *app_name);
	pid_t   proc_to_pid (char *proc_name);
	ssize_t run_cmd_pipe (const char *cmd, char *const cmdv[],
			      char *pbuf, size_t pbuf_size, bool use_shell);
	pid_t   run_cmd_bg (const char *cmd, char *const cmdv[],
			    bool do_wait, bool use_shell, char *preload_lib);
	pid_t   run_pgrp_bg (const char *pcmd, char *const pcmdv[],
			     const char *ccmd, char *const ccmdv[],
			     char *const pid_cmd, char *proc_name,
			     u32 delay, bool do_wait, bool use_shell,
			     char *preload_lib);
	bool    pid_is_running (pid_t pid, char *proc_name, bool use_wait);
	pid_t   fork_proc (void (*task) (void *), void *argp);
	void    kill_proc (pid_t pid);
	void    wait_proc (pid_t pid);
#ifdef __cplusplus
};
#endif

/* Inline functions */
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

#endif
