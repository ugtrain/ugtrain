/* ugptrfind.c:    find vectors and pointers in mem. obj. dump files
 *
 * Copyright (c) 2017 Sebastian Parschauer <s.parschauer@gmx.de>
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>

#define APP_NAME "ugptrfind"

char Help[] =
APP_NAME " - find vectors and pointers in mem. obj. dump files\n"
"\n"
"Usage: " APP_NAME " <file> <region_start> <region_end>\n"
"\n"
"Example: " APP_NAME " 0_000.dump 0x01ebc000 0x0bc9b000 | less\n"
;

/*
 * Limitations: Only searches aligned, doesn't get
 *              region start/end automatically.
 */
int main (int argc, char **argv)
{
	int prev_i = 0, i, fd = -1;
	long start, end, prev_val = 0, val;
	off_t file_end;
	size_t rbytes = 0;

	if (argc != 4) {
		fprintf(stderr, "%s", Help);
		goto err;
	}

	start = strtol(argv[2], NULL, 16);
	if (errno) {
		perror("strtol");
		goto err;
	}
	end = strtol(argv[3], NULL, 16);
	if (errno) {
		perror("strtol");
		goto err;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		goto err;
	}

	file_end = lseek(fd, 0, SEEK_END);
	if (file_end < 0) {
		perror("lseek");
		goto err_close;
	}
	if (lseek(fd, 0, SEEK_SET) < 0) {
		perror("lseek");
		goto err_close;
	}

	for (i = 0; i < file_end; i += rbytes) {
		rbytes = read(fd, &val, sizeof(long));
		if (rbytes != sizeof(long)) {
			perror("read");
			goto err_close;
		}
		if (val >= start && val <= end) {
			if (i != 0 && i - sizeof(long) == prev_i &&
			    val > prev_val)
				printf("%x: 0x%lx <-- vector %ld bytes\n",
				       i, val, val - prev_val);
			else if (i != 0 && i - sizeof(long) == prev_i &&
			    val == prev_val)
				printf("%x: 0x%lx <-- empty vector\n", i, val);
			else
				printf("%x: 0x%lx\n", i, val);
			prev_val = val;
			prev_i = i;
		}
	}

	close(fd);
	return 0;
err_close:
	close(fd);
err:
	return -1;
}
