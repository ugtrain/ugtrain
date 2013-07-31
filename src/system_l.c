/* system_l.c:    provide OS specific helpers (e.g. run cmds)
 *
 * Copyright (c) 2012..13, by:  Sebastian Riemer
 *    All rights reserved.      Ernst-Reinke-Str. 23
 *                              10369 Berlin, Germany
 *                             <sebastian.riemer@gmx.de>
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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include "common.h"
#include "system.h"

#define SHELL "/bin/sh"


i32 fork_wait_kill (pid_t wpid, void (*task) (void *), void *argp)
{
	pid_t pid;
	i32 status;

	pid = fork();
	if (pid < 0) {
		perror("fork");
		goto err;
	} else if (pid == 0) {
		task(argp);
	} else {
		if (wpid < 0) {
			waitpid(pid, &status, 0);
		} else {
			waitpid(wpid, &status, 0);
			kill(pid, SIGINT);
		}
	}
	return 0;
err:
	return -1;
}

i32 run_cmd (const char *cmd, char *const cmdv[])
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		perror("fork");
		goto err;
	} else if (pid == 0) {
		if (execvp(cmd, cmdv) < 0) {
			perror("execvp");
			goto child_err;
		}
	}
	return 0;
err:
	return -1;
child_err:
	exit(-1);
}

ssize_t run_cmd_pipe (const char *cmd, char *const cmdv[], char *pbuf,
		      size_t pbuf_size, u8 use_shell)
{
	pid_t pid;
	i32 status, fds[2];
	ssize_t bytes_read = 0;

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
			fprintf(stderr, "No output from command!\n");
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

	if (run_cmd_pipe(cmd, cmdv, pbuf, sizeof(pbuf), 0) <= 0)
		goto err;

	if (!isdigit(pbuf[0]))
		goto err;

	pid = atoi(pbuf);
	if (pid <= 1)
		goto err;

	return pid;
err:
	fprintf(stderr, "PID not found or invalid!\n");
	return -1;
}
