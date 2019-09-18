/* ugstackdisc.c:    discover the forward stack offset of a value
 *
 * Copyright (c) 2019 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <lib/types.h>

#define APP_NAME "ugstackdisc"
#define STACK_LIMIT (long)(16 * 1024 * 1024)


char Help[] =
APP_NAME " - discover forward stack offset of a stack value\n"
"\n"
"Usage: " APP_NAME " <pid> <value_hex_address> [--debug]\n"
"\n"
"Example: " APP_NAME " 1234 0xdeadbeef\n"
"\n"
"This tool finds the stack end in /proc/$pid/stat and calcules the\n"
"forward stack offset from the provided memory address found by scanmem.\n"
"So the process with that pid should still be running.\n"
;


i32 main (i32 argc, char **argv)
{
	pid_t pid;
	ptr_t addr;
	bool debug_mode = false;

	FILE *stat_file;
	char *line = NULL, *pos, *spos;
	char stat_path[128] = {'\0'};
	size_t len = 0, spaces_in_procname = 0;
	ptr_t stack_offs = 0, stack_end = 0;
	/*
	 *  The stack end is the 28th value. So go to 27th space.
	 *  See "man 5 proc".
	 */
	const i32 stack_end_idx = 27;
	i32 i, ret = -1;

	/* Option parsing */
	if (argc < 3)
		goto err;

	ret = sscanf(argv[1], "%u", &pid);
	if (ret != 1)
		goto err;
	ret = -1;
	errno = 0;
	addr = strtoptr(argv[2], NULL, 16);
	if (errno)
		goto err;
	if (argc >= 4 && strncmp(argv[3], "--debug", sizeof("--debug")) == 0)
		debug_mode = true;

	/* Get stat path */
	if (snprintf(stat_path, sizeof(stat_path), "/proc/%u/stat", pid) <= 0) {
		fprintf(stderr, "Cannot determine the /proc/$pid/stat path.\n");
		return ret;
	}

	/* Open stat file */
	stat_file = fopen(stat_path, "r");
	if (!stat_file) {
		fprintf(stderr, "Failed to open stat file %s.\n", stat_path);
		return ret;
	}
	/* Read stat file */
	if (getline(&line, &len, stat_file) != -1) {
		/* handle spaces in the process name first */
		pos = strchr(line, ')');
		if (!pos)
			goto parse_err;
		*pos = '\0';
		spos = line;
		while (true) {
			spos = strchr(spos + 1, ' ');
			if (spos)
				spaces_in_procname++;
			else
				break;
		}
		if (spaces_in_procname > 0)
			spaces_in_procname--;
		*pos = ')';

		/* get the stack end */
		spos = line;
		for (i = 0; i < (stack_end_idx + spaces_in_procname); i++) {
			spos = strchr(spos + 1, ' ');
			if (!spos)
				goto parse_err;
		}
		pos = strchr(spos + 1, ' ');
		if (!pos)
			goto parse_err;
		*pos = '\0';
		ret = sscanf(spos, "%lu", &stack_end);
		if (ret != 1)
			goto parse_err;
		ret = -1;
	} else {
		if (line)
			free(line);
		fclose(stat_file);
		fprintf(stderr, "Failed to read stat file %s.\n", stat_path);
		return ret;
	}
	free(line);
	fclose(stat_file);

	if (debug_mode) {
		printf("pid: %u, addr: " PRI_PTR ", path: %s, stack_end: "
		       PRI_PTR "\n", pid, addr, stat_path, stack_end);
	}

	/* Calculate, check, and print stack offset */
	stack_offs = stack_end - addr;
	if (stack_offs > STACK_LIMIT) {
		fprintf(stderr, "Stack offset unlikely - exceeds limit of "
			"0x%lx\n", STACK_LIMIT);
		return ret;
	}
	printf(PRI_PTR "\n", stack_offs);

	return EXIT_SUCCESS;
err:
	fprintf(stderr, "%s", Help);
	return ret;
parse_err:
	free(line);
	fclose(stat_file);
	fprintf(stderr, "Failed to parse stat file.\n");
	return ret;
}
