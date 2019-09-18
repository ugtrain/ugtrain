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
#include <lib/maps.h>

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

	LIST_HEAD(_rlist);
	struct list_head *rlist = &_rlist;
	struct pmap_params params;
	ptr_t stack_offs = 0, stack_end = 0, stack_rstart = 0, stack_rend = 0;
	i32 ret = -1;

	params.rlist = rlist;
	params.exe_path = NULL;

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

	/* Get memory regions */
	read_regions(pid, &params);
	find_stack_bounds(params.rlist, &stack_rstart, &stack_rend);
	if (!stack_rstart) {
		fprintf(stderr, "Failed to read the stack bounds.\n");
		goto err_free;
	}
	if (addr < stack_rstart || addr > stack_rend) {
		fprintf(stderr, "Address " PRI_PTR " is out of bounds ("
			PRI_PTR ".." PRI_PTR ").\n", addr,
			stack_rstart, stack_rend);
		goto err_free;
	}

	/* Get stack end */
	stack_end = get_stack_end(pid, stack_rstart, stack_rend);
	if (stack_end == stack_rstart) {
		fprintf(stderr, "Failed to read the stack end.\n");
		goto err_free;
	}

	if (debug_mode) {
		printf("pid: %u, addr: " PRI_PTR ", stack_end: "
		       PRI_PTR "\n", pid, addr, stack_end);
	}

	/* Calculate, check, and print stack offset */
	stack_offs = stack_end - addr;
	if (stack_offs > STACK_LIMIT) {
		fprintf(stderr, "Stack offset unlikely - exceeds limit of "
			"0x%lx\n", STACK_LIMIT);
		goto err_free;
	}
	printf(PRI_PTR "\n", stack_offs);

	rlist_clear(rlist);
	return EXIT_SUCCESS;
err:
	fprintf(stderr, "%s", Help);
	return ret;
err_free:
	rlist_clear(rlist);
	return ret;
}
