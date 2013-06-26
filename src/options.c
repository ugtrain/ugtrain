/* options.c:    option parsing, help, usage, etc.
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
#include "options.h"

const char Help[] =
PROG_NAME " is an universal game trainer for the CLI\n"
"\n"
"Usage: " PROG_NAME " [opts] <config_path>\n"
"\n"
"--help, -h:		show this information\n"
"--adapt, -A:		run the adaption script from config to determine\n"
"			code addresses (not allowed for root)\n"
"--preload, -P <lib>:	start the game preloaded with the given lib\n"
"			(ignored for root)\n"
;

void usage()
{
	fprintf(stderr, "%s", Help);
	exit(-1);
}

static const char short_options[] = "-hAP:";
static struct option long_options[] = {
	{"help",           0, 0, 'h'},
	{"adapt",          0, 0, 'A'},
	{"preload",        1, 0, 'P'},
	{0, 0, 0, 0}
};

void parse_options (int argc, char **argv, struct app_options *opt)
{
	int ch, opt_idx;
	opt->do_adapt = 0;
	opt->preload_lib = NULL;

	while (1) {
		ch = getopt_long (argc, argv, short_options,
				  long_options, &opt_idx);
		if (ch < 0)
			break;

		switch (ch) {
		case 'h':
			usage();
			break;
		case 'A':
			opt->do_adapt = 1;
			break;
		case 'P':
			if (optind == argc)
				usage();
			opt->preload_lib = optarg;
			break;
		default:
			if (optind != argc)
				usage();
			break;
		}
	}
}
