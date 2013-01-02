/* ugpreload.c:    preloader to hook a lib into a victim process
 *
 * Copyright (c) 2013, by:      Sebastian Riemer
 *    All rights reserved.      Ernst-Reinke-Str. 23
 *                              10369 Berlin, Germany
 *                             <sebastian.riemer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

/* env var to be added */
#define PRELOAD_VAR "LD_PRELOAD"

typedef unsigned long long u64;
typedef unsigned int u32;

#ifdef __i386__
typedef u32 ptr_t;
#else
typedef u64 ptr_t;
#endif

#define PTR_ADD(type, x, y)   (type) ((ptr_t)x + (ptr_t)y)

#define PATH_MAX 4096

#if 0
void print_env (char **envp)
{
	int i;

	for (i = 0; envp[i] != NULL; i++)
		printf("%s\n", envp[i]);
}
#endif

/*
 * can't rely on "envz_add" or "setenv" - these crash
 *
 * This function removes an old occurence of the requested
 * env var, appends the requested env value and returns the
 * new env length.
 *
 * You need to preallocate the additionally needed memory,
 * you need to know the envp length, the string count and
 * where to remove the env value to be added if it is
 * already set.
 */
int my_env_add (char **envp, size_t *env_len, int count,
		const char *env_entry, int rpos, size_t rlen)
{
	int i;

	if (rlen > 0) {
		/* move memory to overwrite the old value
		   and update pointers */
		memmove(envp[rpos], envp[rpos] + rlen,
			*env_len - (envp[rpos] - envp[0]) - rlen);
		for (i = rpos + 1; i <= count; i++)
			envp[i-1] = envp[i] - rlen;
		i = count - 1;
		envp[i+1] = NULL;
	} else {
		rlen = 0;
		i = count;
	}

	/* append the new env value */
	memcpy(envp[i], env_entry, strlen(env_entry) + 1);

	/* update the env length */
	*env_len += strlen(env_entry) + 1 - rlen;

	return 0;
}

int main (int argc, char *argv[], char *envp[])
{
	char preload_str[PATH_MAX] = PRELOAD_VAR "=";
	u32 preload_str_size = 0;
	char *game_path = NULL;
	int i, rpos = 0, count = 0, status;
	pid_t pid;
	size_t rlen = 0, env_len = 0, new_len = 0, str_len = 0, str_start = 0;
	char **new_envp = NULL;

	if (argc < 3) {
		fprintf(stderr, "use the following parameters: "
			"<lib_path> <app_path>\n");
		return 1;
	}

	preload_str_size = sizeof(PRELOAD_VAR "=") + strlen(argv[1]);
	if (preload_str_size > PATH_MAX) {
		fprintf(stderr, "The lib_path is too long.\n");
		return 1;
	}
	strcat(preload_str, argv[1]);
	game_path = argv[2];

	/* get length of env and check if env var
	   to be added must be overwritten */
	for (i = 0; envp[i] != NULL; i++) {
		env_len += strlen(envp[i]) + 1;
		if (!strncmp(envp[i], PRELOAD_VAR, strlen(PRELOAD_VAR))) {
			rpos = i;
			rlen = strlen(envp[i]) + 1;
		}
	}
	count = i;

	/* prepare heap memory for string pointers followed by strings,
	   add the needed space to insert our LD_PRELOAD string */
	str_start = (count+2) * sizeof(char *);
	new_len = str_start + env_len + preload_str_size;
	new_envp = (char **) malloc(new_len);
	if (!new_envp) {
		perror("malloc");
		return 1;
	}

	/* set string pointer first, then copy env string from stack
	   to bigger heap for data insertion */
	for (i = 0; i < count; i++) {
		new_envp[i] = PTR_ADD(char *, &new_envp[0],
				      str_start + str_len);
		strcpy(new_envp[i], envp[i]);
		str_len += strlen(envp[i]) + 1;
	}
	new_envp[i] = PTR_ADD(char *, &new_envp[0],
			      str_start + str_len);   /* for string to be inserted */
	new_envp[i+1] = NULL; /* end of env */

	/* add the new env val to the array */
	my_env_add(new_envp, &env_len, count, preload_str, rpos, rlen);


	/* prepare memory hacking */
	if (mkfifo("/tmp/memhack_in", S_IRUSR | S_IWUSR) < 0 && errno != EEXIST)
		perror("input mkfifo");

	if (mkfifo("/tmp/memhack_out", S_IRUSR | S_IWUSR) < 0 && errno != EEXIST)
		perror("output mkfifo");

	/* run the victim process */
	pid = fork();
	if (pid == 0) {
		printf("fork worked\n");
		if (execle(game_path, game_path, NULL, new_envp) < 0) {
			perror("execle");
			return 1;
		}
	} else if (pid > 0) {
		waitpid(pid, &status, 0);
		printf("child status: %d\n", status);
	} else {
		perror("fork");
	}

	if (new_envp)
		free(new_envp);

	return EXIT_SUCCESS;
}
