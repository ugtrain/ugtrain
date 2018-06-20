/* system.h:    provide OS specific helpers (e.g. run cmds)
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef SYSTEM_H
#define SYSTEM_H

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef __linux__
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/stat.h>
#else
#include <windows.h>
#endif

/* local includes */
#include "types.h"


/* states returned by check_process() */
enum pstate {
	PROC_RUNNING,
	PROC_ERR,  /* handled like running */
	PROC_DEAD,
	PROC_WRONG,
	PROC_ZOMBIE
};

#ifdef __cplusplus
extern "C" {
#endif
	char    *get_abs_app_path (char *app_name);
	pid_t   proc_to_pid (char *proc_name);
	ssize_t run_cmd_pipe (const char *cmd, char *const cmdv[],
			      char *pbuf, size_t pbuf_size);
	pid_t   run_cmd_bg (const char *cmd, char *const cmdv[],
			    bool do_wait, char *preload_lib);
	pid_t   run_pgrp_bg (const char *pcmd, char *const pcmdv[],
			     const char *ccmd, char *const ccmdv[],
			     char *const pid_cmd, char *proc_name,
			     u32 delay, bool do_wait, char *preload_lib);
	enum pstate check_process (pid_t pid, char *proc_name);
	pid_t   fork_proc (void (*task) (void *), void *argp);
#ifdef __cplusplus
};
#endif

/* Inline functions */
static inline pid_t run_cmd (const char *cmd, char *const cmdv[])
{
	return run_cmd_bg(cmd, cmdv, true, NULL);
}

#ifdef __linux__
/* Path separator */
#define PSEP "/"
/* Prefix to hide a file or directory */
#define PHIDE "."

static inline void get_home_path (char **home)
{
	*home = secure_getenv("HOME");
}

static inline bool dir_exists (const char *path)
{
	DIR *dir;

	dir = opendir(path);
	if (dir) {
		closedir(dir);
		return true;
	} else if (errno == ENOENT) {
		return false;
	} else {
		/* assume that dir does not exist */
		return false;
	}
}

static inline i32 create_dir (const char *path)
{
	return mkdir(path, 0755);
}

static inline bool file_exists (const char *path)
{
	struct stat buf;
	return !(stat(path, &buf));
}

static inline i32 rm_file (const char *path)
{
	return unlink(path);
}

static inline i32 rm_files_by_pattern (char *pattern)
{
	char cmd_str[PIPE_BUF] = { 0 };

	/* using the shell is required */
	snprintf(cmd_str, PIPE_BUF - 1, "rm -f %s", pattern);
	if (run_cmd(cmd_str, NULL) < 0)
		return -1;
	else
		return 0;
}

static inline void sleep_sec (u32 sec)
{
	sleep(sec);
}

static inline void sleep_msec (u32 msec)
{
	usleep(msec * 1000);
}

/*
 * sleep given seconds count unless there is input on given fd
 *
 * Not using va_list to keep this inline. Assumption: fd is valid.
 */
static inline void sleep_sec_unless_input (u32 sec, i32 fd)
{
	fd_set fs;
	struct timeval tv;
	i32 nfds = fd + 1;

	FD_ZERO(&fs);
	tv.tv_sec = sec;
	tv.tv_usec = 0;

	FD_SET(fd, &fs);

	select(nfds, &fs, NULL, NULL, &tv);
}

static inline void sleep_msec_unless_input (u32 msec, i32 fd)
{
	fd_set fs;
	struct timeval tv;
	i32 nfds = fd + 1;

	FD_ZERO(&fs);
	tv.tv_sec = msec / 1000;
	tv.tv_usec = (msec % 1000) * 1000;

	FD_SET(fd, &fs);

	select(nfds, &fs, NULL, NULL, &tv);
}

/*
 * sleep given seconds count unless there is input on given fds
 *
 * Not using va_list to keep this inline. Assumption: fds are valid.
 */
static inline void sleep_sec_unless_input2 (u32 sec, i32 fd1, i32 fd2)
{
	fd_set fs;
	struct timeval tv;
	i32 nfds = (fd2 > fd1) ? fd2 + 1 : fd1 + 1;

	FD_ZERO(&fs);
	tv.tv_sec = sec;
	tv.tv_usec = 0;

	FD_SET(fd1, &fs);
	FD_SET(fd2, &fs);

	select(nfds, &fs, NULL, NULL, &tv);
}

/*
 * wait more reliably than waitpid() as the game process
 * may be forked off or belong to init
 */
static inline void wait_orphan (pid_t pid, char *proc_name)
{
	enum pstate pstate;
	while (true) {
		pstate = check_process(pid, proc_name);
		if (pstate != PROC_RUNNING && pstate != PROC_ERR)
			return;
		sleep_sec(1);
	}
}

static inline void wait_proc (pid_t pid)
{
	i32 status;

	waitpid(pid, &status, 0);
}

static inline void kill_proc (pid_t pid)
{
	kill(pid, SIGTERM);
}

static inline void ignore_sigint (void)
{
	signal(SIGINT, SIG_IGN);
}

/* set SIGINT handler to default action */
static inline void reset_sigint (void)
{
	signal(SIGINT, SIG_DFL);
}

static inline void set_sigterm_handler (void (*sighandler)(i32))
{
	signal(SIGTERM, sighandler);
}

#else

/* Path separator */
#define PSEP "\\"
#define PHIDE ""

static inline void get_home_path (char **home)
{
	const char def_home[] = "C:";

	*home = strdup(def_home);
}

static inline bool dir_exists (const char *path)
{
	return false;
}

static inline i32 create_dir (const char *path)
{
	return -1;
}

static inline bool file_exists (const char *path)
{
	return false;
}

static inline i32 rm_file (const char *path)
{
	return -1;
}

static inline i32 rm_files_by_pattern (char *pattern)
{
	return -1;
}

static inline void sleep_sec (u32 sec)
{
	Sleep(sec * 1000);
}

static inline void sleep_msec (u32 msec)
{
	Sleep(msec);
}

#ifndef STDIN_FILENO
#define STDIN_FILENO -1
#endif
static inline void sleep_sec_unless_input (u32 sec, i32 fd)
{
	sleep_sec(sec);
}
static inline void sleep_msec_unless_input (u32 msec, i32 fd)
{
	sleep_msec(msec);
}
static inline void sleep_sec_unless_input2 (u32 sec, i32 fd1, i32 fd2)
{
	sleep_sec(sec);
}

static inline void wait_orphan (pid_t pid, char *proc_name)
{
}

static inline void wait_proc (pid_t pid)
{
}

static inline void kill_proc (pid_t pid)
{
}

static inline void ignore_sigint (void)
{
}

static inline void reset_sigint (void)
{
}

static inline void set_sigterm_handler (void (*sighandler)(i32))
{
}

#endif

#endif
