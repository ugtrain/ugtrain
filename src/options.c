/* options.c:    option parsing, help, usage, etc.
 *
 * Copyright (c) 2012..14, by:  Sebastian Parschauer
 *    All rights reserved.     <s.parschauer@gmx.de>
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
#include <getopt.h>
#include <string.h>
#include "options.h"


#define DYNMEM_FILE "/tmp/memhack_file"

void do_assumptions (struct app_options *opt)
{
	/* Adaption handling */
	/* '-D 1 -A' --> '-D 1', '-S -A' --> '-S' */
	if ((opt->disc_str && opt->disc_str[0] != '5') || opt->run_scanmem)
		opt->do_adapt = false;

	if (opt->do_adapt) {
		/* '-A' --> '-A -D 5' */
		if (!opt->disc_str)
			opt->disc_str = (char *) "5";
		/* '-P libmemhack*' -> '' */
		if (opt->preload_lib && strncmp(opt->preload_lib, LHACK_PRE,
		    sizeof(LHACK_PRE) - 1) == 0)
			opt->preload_lib = NULL;
	}

	/* Preloading/Starting handling */
	if (!opt->preload_lib) {
		/* '-D <str>' --> '-D <str> -P libmemdisc32/64.so' */
		if (opt->disc_str) {
			use_libmemdisc(opt);
		/* '-S' --> '-S -P libmemhack32/64.so',
		 * '--glc' --> '--glc -P libmemhack32/64.so' */
		} else if (opt->run_scanmem || opt->pre_cmd) {
			use_libmemhack(opt);
		}
	}
}

static const char Help[] =
PROG_NAME " is the universal elite game trainer for the CLI\n"
"\n"
"Usage: " PROG_NAME " [opts] <config_path>\n"
"\n"
"--help, -h:		show this information\n"
"--version, -V:		show version information and exit\n"
"\n"
"Trainer Options\n"
"--preload, -P [lib]:	start the game preloaded with the given lib -\n"
"			without argument, \'-P " LHACK_PRE "32/64"
				LIB_END "\' is assumed\n"
"			(depending on \'sizeof(void *)\', ignored for root)\n"
"			- starts the game for pure static memory as well\n"
"--discover, -D <str>:	do discovery by sending the string to the discovery\n"
"			library - without \'-P\', \'-P "
				LDISC_PRE "32/64" LIB_END "\'\n"
"			is assumed depending on \'sizeof(void *)\'\n"
"--scanmem, -S:		run \'scanmem\' without/together with \'-D\' in the\n"
"			same process group as the game (avoid root access)\n"
"			- assumes \'-P\' and ignores the config\n"
"--adapt, -A:		run the adaption script from the config to determine\n"
"			malloc code addresses and discover the stack offsets\n"
"			- assumes \'-D 5\' (stage 5: finding stack offset)\n"
"			using the new code address (rejects if root)\n"
"Misc Options\n"
"--pre-cmd=<str>:	together with the assumed \'-P\' it is possible to\n"
"			specify a preloader command like \'"
				GLC_PRELOADER "\'\n"
"			- it is only important that it appends to LD_PRELOAD\n"
"--glc[=str]:		run \'" GLC_PRELOADER "\' with the given options for\n"
"			video recording while cheating - assumes \'-P\'\n"
"\n"
"Report bugs to and ask " PACKAGE_BUGREPORT " for more help!\n"
;

static void usage()
{
	fprintf(stderr, "%s", Help);
	exit(-1);
}

/* use non-printable starting at > 8-bit char table */
enum {
	PreCmd = 300,
	Glc,
	Dietmar,
};

static const char short_options[] = "-hVAD:P::S";
static struct option long_options[] = {
	{"help",           0, 0, 'h'},
	{"version",        0, 0, 'V'},
	{"adapt",          0, 0, 'A'},
	{"discover",       1, 0, 'D'},
	{"preload",        2, 0, 'P'},
	{"scanmem",        0, 0, 'S'},
	{"pre-cmd",        1, 0, PreCmd },
	{"glc",            2, 0, Glc },
	{"dietmar",        0, 0, Dietmar },
	{0, 0, 0, 0}
};

static void init_options (struct app_options *opt)
{
	memset(opt, 0, sizeof(struct app_options));

	/* no direct CLI input */
	opt->scanmem_pid = -1;
	opt->dynmem_file = DYNMEM_FILE;
}

/*
 * parses the command-line options
 */
void parse_options (i32 argc, char **argv, struct app_options *opt)
{
	i32 ch = -1, prev_ch = -1, opt_idx = 0;

	if (argc < 2)
		usage();

	init_options(opt);

	while (true) {
		prev_ch = ch;
		ch = getopt_long (argc, argv, short_options,
				  long_options, &opt_idx);
		if (ch < 0)
			break;

		switch (ch) {
		case 'h':
			usage();
			break;
		case 'V':
			printf(PACKAGE_STRING " (%s %s)\n\nPlease report "
				"bugs to " PACKAGE_BUGREPORT "!\n",
				__DATE__, __TIME__);
			exit(0);
		case 'A':
			opt->do_adapt = true;
			break;
		case 'D':
			if (optind == argc)
				usage();
			opt->disc_str = optarg;
			break;
		case 'P':
			if (optind == argc || !optarg)
				use_libmemhack(opt);
			else
				opt->preload_lib = optarg;
			break;
		case 'S':
			opt->run_scanmem = true;
			break;
		case PreCmd:
			opt->pre_cmd = optarg;
			break;
		case Glc:
			if (optind == argc || !optarg)
				opt->pre_cmd = "";
			else
				opt->pre_cmd = optarg;
			opt->use_glc = true;
			break;
		case Dietmar:
			printf(PACKAGE_STRING " is dedicated to Dietmar.\n"
				"He could have used some extra health.\n"
				"Died: 2013-08-01 at the age of 63 in his "
				"car.\nWe miss you!\n");
			exit(0);
			break;
		default:  /* unknown option */
			if (optind != argc) {
				/* optional argument handling */
				switch (prev_ch) {
				case 'P':
					opt->preload_lib = argv[optind - 1];
					break;
				case Glc:
					opt->pre_cmd = argv[optind - 1];
					break;
				default:
					usage();
					break;
				}
			} else {
				opt->cfg_path = argv[optind - 1];
			}
			break;
		}
	}
	if (!opt->cfg_path)
	       usage();

	do_assumptions(opt);
}
