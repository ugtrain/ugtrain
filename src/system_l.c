/* system_l.c:    provide OS specific helpers (e.g. run cmds)
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

#ifdef __linux__

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <dirent.h>
#include <libgen.h>
#include <errno.h>
#include "common.h"
#include "system.h"
#include "preload.h"

#define SHELL "/bin/sh"


/*
 * Run a task in background.
 *
 * Parameters: task function pointer,
 *             void *argument for the task
 * Returns: pid for success, -1 for failure
 *
 * Please note: The task has to cast the argument pointer to its
 *              correct type again. If more arguments are required,
 *              please use a structure.
 */
pid_t fork_proc (void (*task) (void *), void *argp)
{
	pid_t pid = -1;

	pid = fork();
	if (pid < 0) {
		perror("fork");
		goto err;
	} else if (pid == 0) {
		task(argp);
	}
	return pid;
err:
	return -1;
}

/*
 * If requested, we detect if a child process is running
 * with the waitpid() option WNOHANG.
 *
 * Otherwise, we check if a process is running in /proc directly
 * and we handle zombies as if the process is not running.
 *
 * Assumption: (pid > 0)  --> Please check your PID before!
 */
bool pid_is_running (pid_t call_pid, pid_t pid, char *proc_name, bool use_wait)
{
	char pbuf[PIPE_BUF] = { 0 };
	i32 status;
	DIR *dir;
	const char *cmd;
	char *pname;
	char cmd_str[1024] = "/proc/";
	i32 pr_len, cmd_len = sizeof("/proc/") - 1;

	if (use_wait) {
		if (waitpid(call_pid, &status, WNOHANG|WUNTRACED|WCONTINUED) !=
		    call_pid)
			return true;
		return !(WIFEXITED(status) || WIFSIGNALED(status));
	}

	/* append pid and check if dir exists */
	pr_len = sprintf((cmd_str + cmd_len), "%d", pid);
	if (pr_len <= 0)
		goto err;
	cmd_len += pr_len;
	dir = opendir(cmd_str);
	if (!dir)
		return (errno == ENOENT) ? false : true;
	else
		closedir(dir);

	/* check process name from 'exe' symlink */
	pr_len = sprintf((cmd_str + cmd_len), "/exe");
	if (pr_len <= 0)
		goto err;
	if (readlink(cmd_str, pbuf, sizeof(pbuf)) <= 0)
		goto skip_exe_check;  /* skip for zombies */
	pname = basename(pbuf);
	if (!pname)
		goto err;
	if (strcmp(pname, proc_name) != 0)
		return false;

skip_exe_check:
	/* remove '/exe', insert 'cat ', append '/status' + shell cmds */
	memset(cmd_str + cmd_len, 0, pr_len);
	pr_len = sizeof("cat ") - 1;
	memmove(cmd_str + pr_len, cmd_str, cmd_len);
	pr_len = sprintf(cmd_str, "cat %s", (cmd_str + pr_len));
	if (pr_len <= 0)
		goto err;
	cmd_len = pr_len;
	pr_len = sprintf((cmd_str + cmd_len),
		"/status | sed -n 2p | sed \'s/State:	//g\'");
	if (pr_len <= 0)
		goto err;
	cmd = cmd_str;

	/* reset pipe buffer and run the shell cmd */
	memset(pbuf, 0, sizeof(pbuf));
	if (run_cmd_pipe(cmd, NULL, pbuf, sizeof(pbuf)) <= 0)
		goto err;

	/* zombies are not running - parent doesn't wait */
	if (pbuf[0] == 'Z')
		return false;
err:
	return true;  /* In error case we expect the process still running. */
}

/*
 * This is highly specific to bypass OS security.
 *
 * E.g. ptrace() requires root for processes not running
 * in the same process group. The trick is now to fork a
 * process which forks and executes the game. This process
 * is the parent of the game and parents may ptrace their
 * children. So it executes the ptrace program like scanmem
 * right after that. The user doesn't require root anymore.
 * This is elite!
 *
 * pstree example: ugtrain---scanmem---chromium-bsu
 *
 * Note: We have to find out the PID of the game ourselves
 *       and the PID may be required for the ptrace command.
 *       So ensure that pid_str has space for at least 12 chars
 *       and is part of pcmdv[]! Also give us the process name!
 *       Only the child command (the game) can run in a shell.
 *
 * ATTENTION: The child becomes an orphan when ending the parent before
 *            the child! We can't wait for it as we have to exec the
 *            parent command. An orphan catcher is required!
 */
pid_t run_pgrp_bg (const char *pcmd, char *const pcmdv[],
		   const char *ccmd, char *const ccmdv[],
		   char *const pid_str, char *proc_name,
		   u32 delay, bool do_wait, char *preload_lib)
{
	pid_t ppid, cpid, game_pid = -1;
	i32 status;
	bool use_shell = (ccmdv == NULL);

	if (!pid_str || !proc_name)
		goto err;

	ppid = fork();
	if (ppid < 0) {
		perror("fork");
		goto err;
	} else if (ppid == 0) {
		cpid = fork();
		if (cpid < 0) {
			perror("fork");
			goto child_err;
		} else if (cpid == 0) {   /* child command (the game) */
			if (preload_library(preload_lib))
				goto child_err;
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			if (use_shell && execlp(SHELL, SHELL, "-c",
			    ccmd, NULL) < 0) {
				perror("execlp");
				goto child_err;
			} else if (execvp(ccmd, ccmdv) < 0) {
				perror("execvp");
				goto child_err;
			}
		} else {   /* parent command (likely uses ptrace()) */
			if (delay > 0)
				sleep_sec(delay);
			game_pid = proc_to_pid(proc_name);
			if (game_pid > 0)
				sprintf(pid_str, "%d", game_pid);
			if (execvp(pcmd, pcmdv) < 0) {
				perror("execvp");
				goto child_err;
			}
		}
	} else if (do_wait) {
		waitpid(ppid, &status, 0);
		if ((WIFEXITED(status) && WEXITSTATUS(status)) ||
		    WIFSIGNALED(status))
			goto err;
	}
	return ppid;
err:
	return -1;
child_err:
	exit(-1);
}

/*
 * Run a command with execvp in background.
 *
 * Parameters: execvp params, wait for the process 0/1,
 *             run the command in a shell 0/1, preload_lib
 * Returns: the pid for success, -1 for failure
 *
 * Please note: If the shell is used, then execlp is
 *              used with cmd and cmdv[] is ignored.
 */
pid_t run_cmd_bg (const char *cmd, char *const cmdv[], bool do_wait,
		  char *preload_lib)
{
	pid_t pid;
	i32 status;
	bool use_shell = (cmdv == NULL);

	pid = fork();
	if (pid < 0) {
		perror("fork");
		goto err;
	} else if (pid == 0) {
		if (preload_library(preload_lib))
			goto child_err;
		if (use_shell && execlp(SHELL, SHELL, "-c", cmd, NULL) < 0) {
			perror("execlp");
			goto child_err;
		} else if (execvp(cmd, cmdv) < 0) {
			perror("execvp");
			goto child_err;
		}
	} else if (do_wait) {
		waitpid(pid, &status, 0);
		if ((WIFEXITED(status) && WEXITSTATUS(status)) ||
		    WIFSIGNALED(status))
			goto err;
	}
	return pid;
err:
	return -1;
child_err:
	exit(-1);
}

/*
 * Run a command in the background, wait for it
 * and get its result string from a pipe.
 *
 * Parameters: execvp params, pipe buffer, its size,
 *             run the command in a shell 0/1
 * Returns: what read() returns or -1
 *
 * Please note: If the shell is used, then execlp is
 *              used with cmd and cmdv[] is ignored.
 */
ssize_t run_cmd_pipe (const char *cmd, char *const cmdv[], char *pbuf,
		      size_t pbuf_size)
{
	pid_t pid;
	i32 status, fds[2];
	ssize_t bytes_read = 0;
	bool use_shell = (cmdv == NULL);

	if (pipe(fds) < 0) {
		perror("pipe");
		goto err;
	}
	pid = fork();
	if (pid < 0) {
		perror("fork");
		close(fds[STDOUT_FILENO]);
		close(fds[STDIN_FILENO]);
		goto err;
	} else if (pid == 0) {
		if (dup2(fds[STDOUT_FILENO], STDOUT_FILENO) < 0) {
			perror("dup2");
			close(fds[STDOUT_FILENO]);
			close(fds[STDIN_FILENO]);
			goto child_err;
		}
		close(fds[STDIN_FILENO]);
		if (use_shell && execlp(SHELL, SHELL, "-c", cmd, NULL) < 0) {
			perror("execlp");
			close(fds[STDOUT_FILENO]);
			goto child_err;
		} else if (execvp(cmd, cmdv) < 0) {
			perror("execvp");
			close(fds[STDOUT_FILENO]);
			goto child_err;
		}
	} else {
		close(fds[STDOUT_FILENO]);
		waitpid(pid, &status, 0);
		bytes_read = read(fds[STDIN_FILENO], pbuf, pbuf_size);
		if (bytes_read < 0) {
			perror("pipe read");
			goto parent_err;
		} else if (bytes_read <= 1) {
			goto parent_err;
		}
	}
	close(fds[STDIN_FILENO]);
	return bytes_read;

parent_err:
	close(fds[STDIN_FILENO]);
err:
	return -1;
child_err:
	exit(-1);
}

/*
 * Get the pid of a process name.
 *
 * We always get the first pid we find in case
 * of multiple instances.
 *
 * Parameters: the process name
 * Returns: the pid or -1
 */
pid_t proc_to_pid (char *proc_name)
{
	pid_t pid;
	char pbuf[PIPE_BUF] = { 0 };
	const char *cmd = (const char *) "pidof";
	char *cmdv[4];

	cmdv[0] = (char *) "pidof";
	cmdv[1] = (char *) "-s";
	cmdv[2] = proc_name;
	cmdv[3] = NULL;

	if (run_cmd_pipe(cmd, cmdv, pbuf, sizeof(pbuf)) <= 0)
		goto err;

	if (!isdigit(pbuf[0]))
		goto err;

	pid = atoi(pbuf);
	if (pid <= 1)
		goto err;

	return pid;
err:
	return -1;
}

/*
 * Get the absolute path of an application.
 *
 * Parameters: the application name
 * Returns: the absolute path or NULL
 */
char *get_abs_app_path (char *app_name)
{
	char pbuf[PIPE_BUF] = { 0 };
	const char *cmd = (const char *) "which";
	char *cmdv[3];
	char *abs_path = NULL;
	ssize_t rbytes;

	cmdv[0] = (char *) "which";
	cmdv[1] = app_name;
	cmdv[2] = NULL;

	rbytes = run_cmd_pipe(cmd, cmdv, pbuf, sizeof(pbuf));
	if (rbytes <= 0)
		goto err;

	abs_path = malloc(rbytes + 1);
	if (abs_path) {
		memcpy(abs_path, pbuf, rbytes);
		abs_path[rbytes] = '\0';
		if (abs_path[rbytes - 1] == '\n')
			abs_path[rbytes - 1] = '\0';
	}

	return abs_path;
err:
	fprintf(stderr, "Absolute path not found or invalid!\n");
	return NULL;
}

#endif
