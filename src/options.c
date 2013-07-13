/* options.c:    option parsing, help, usage, etc.
 *
 * Copyright (c) 2013, by:      Sebastian Riemer
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
#include "options.h"

#define LDISC_PRE "libmemdisc"
#define LIB_END   ".so"

const char Help[] =
PROG_NAME " is an universal game trainer for the CLI\n"
"\n"
"Usage: " PROG_NAME " [opts] <config_path>\n"
"\n"
"--help, -h:		show this information\n"
"\n"
"Dynamic Memory Options\n"
"--preload, -P <lib>:	start the game preloaded with the given lib\n"
"			(ignored for root)\n"
"--discover, -D <str>:	do discovery by sending the string to the discovery\n"
"			library - without \'-P\' \'-P "
				LDISC_PRE "32/64" LIB_END "\'\n"
"			is assumed depending on \'sizeof(long)\'\n"
"--adapt, -A:		run the adaption script from config to determine\n"
"			code addresses and their stack offsets - without\n"
"			\'-D\' discovery step 4 (finding stack offset) is\n"
"			assumed	using the new code address (rejects if root)\n"
;

void usage()
{
	fprintf(stderr, "%s", Help);
	exit(-1);
}

static const char short_options[] = "-hAD:P:";
static struct option long_options[] = {
	{"help",           0, 0, 'h'},
	{"adapt",          0, 0, 'A'},
	{"discover",       1, 0, 'D'},
	{"preload",        1, 0, 'P'},
	{0, 0, 0, 0}
};

void do_assumptions (struct app_options *opt)
{
	/* '-A' --> '-A -D 4' */
	if (opt->do_adapt && !opt->disc_str)
		opt->disc_str = (char *) "4";
	/* '-D <str>' --> '-D <str> -P libmemdisc32/64.so' */
	if (opt->disc_str && !opt->preload_lib) {
		if (sizeof(long) == 8)
			opt->preload_lib = (char *) LDISC_PRE "64" LIB_END;
		else if (sizeof(long) == 4)
			opt->preload_lib = (char *) LDISC_PRE "32" LIB_END;
	}
}

static void init_options (struct app_options *opt)
{
	opt->do_adapt = 0;
	opt->preload_lib = NULL;
	opt->disc_str = NULL;
	/* no direct CLI input */
	opt->home = NULL;
	opt->proc_name = NULL;
	opt->adp_script = NULL;
}

void parse_options (int argc, char **argv, struct app_options *opt)
{
	int ch, opt_idx;

	init_options(opt);

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
		case 'D':
			if (optind == argc)
				usage();
			opt->disc_str = optarg;
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
	do_assumptions(opt);
}
