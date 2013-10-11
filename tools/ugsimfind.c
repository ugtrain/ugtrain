/* ugsimfind.c:    find similarities in mem. obj. dump files
 *
 * Copyright (c) 2013, by:      Sebastian Riemer
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include "../src/common.h"

/* always match these */
#define MAX_FILES	 100
#define MAX_FILES_STR	"100"

#define APP_NAME "ugsimfind"


char Help[] =
APP_NAME " - find similarities in mem. obj. dump files\n"
"\n"
"Usage: " APP_NAME " <file0> [file1] ...\n"
"\n"
"Upto " MAX_FILES_STR " files are possible.\n"
"\n"
"Example: " APP_NAME " 0_*.dump\n"
;


i32 main (i32 argc, char **argv)
{
	i32 fds[MAX_FILES];
	i32 i, j, k, l;
	u8 ch;
	bool found;
	u8 values[MAX_FILES];
	u8 counts[MAX_FILES];
	u8 files[MAX_FILES][MAX_FILES]; /* argv idx for file name */
	ssize_t rbytes;
	size_t fidx = 0, diffs = 0;

	if (argc < 2) {
		fprintf(stderr, "%s", Help);
		return -1;
	} else if (argc >= MAX_FILES) {
		fprintf(stderr, "Too many files!\n");
		return -1;
	}

	for (i = 1; i < argc; i++) {
		j = i - 1;
		/* printf("opening %s\n", argv[i]); */
		fds[j] = open(argv[i], O_RDONLY);
		if (fds[j] < 0)
			return -1;
	}
	while (1) {
		diffs = 0;
		memset(values, 0, sizeof(values));
		memset(counts, 0, sizeof(counts));
		memset(files, 0, sizeof(files));
		for (i = 1; i < argc; i++) {
			j = i - 1;
			rbytes = read(fds[j], &ch, sizeof(ch));
			if (rbytes < sizeof(ch))
				return -1;
			found = false;
			for (k = 0; k < diffs; k++) {
				if (ch == values[k]) {
					counts[k]++;
					found = true;
					for (l = 0; l < MAX_FILES; l++) {
						if (files[k][l] == 0) {
							files[k][l] = i;
							break;
						}
					}
					break;
				}
			}
			if (!found) {
				values[k] = ch;
				counts[k]++;
				diffs++;
				for (l = 0; l < MAX_FILES; l++) {
					if (files[k][l] == 0) {
						files[k][l] = i;
						break;
					}
				}
			}
		}

		printf("\n\nfidx: 0x%x\n", (int) fidx);
		for (k = 0; k < diffs; k++) {
			printf("0x%02x \'%c\' : %u\t", (int) values[k],
			       isprint((int) values[k]) ? (int) values[k] : '.',
			       counts[k]);
			for (l = 0; l < MAX_FILES; l++) {
				if (files[k][l] == 0)
					break;
				printf("%s, ", argv[files[k][l]]);
			}
			printf("\n");
		}

		fidx++;
	}

	return EXIT_SUCCESS;
}
