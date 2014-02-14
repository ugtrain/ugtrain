/* options.c:    option parsing, help, usage, etc.
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
#include <getopt.h>
#include <string.h>
#include "options.h"

#define LDISC_PRE "libmemdisc"
#define LHACK_PRE "libmemhack"
#define LIB_END   ".so"

const char Help[] =
PROG_NAME " is the universal elite game trainer for the CLI\n"
"\n"
"Usage: " PROG_NAME " [opts] <config_path>\n"
"\n"
"--help, -h:		show this information\n"
"\n"
"Dynamic Memory Options\n"
"--preload, -P [lib]:	start the game preloaded with the given lib -\n"
"			without argument \'-P " LHACK_PRE "32/64"
				LIB_END "\' is assumed\n"
"			(depending on \'sizeof(long)\', ignored for root)\n"
"			This starts the game for pure static memory as well.\n"
"--discover, -D <str>:	do discovery by sending the string to the discovery\n"
"			library - without \'-P\' \'-P "
				LDISC_PRE "32/64" LIB_END "\'\n"
"			is assumed depending on \'sizeof(long)\'\n"
"--scanmem, -S:		run \'scanmem\' without/together with \'-D\' in the\n"
"			same process group as the game (avoid root access)\n"
"--adapt, -A:		run the adaption script from the config to determine\n"
"			code addresses and their stack offsets - without\n"
"			\'-D\' discovery stage 5 (finding stack offset) is\n"
"			assumed	using the new code address (rejects if root)\n"
"--pre-cmd=<str>:	together with \'-P\' it is possible to specify a\n"
"			custom preloader command like \'" GLC_PRELOADER "\'\n"
"--glc[=str]:		run \'" GLC_PRELOADER "\' instead of \'"
				PRELOADER "\' with the\n"
"			given options for video recording while cheating\n"
;

void usage()
{
	fprintf(stderr, "%s", Help);
	exit(-1);
}

static const char short_options[] = "-hAD:P::S";
static struct option long_options[] = {
	{"help",           0, 0, 'h'},
	{"adapt",          0, 0, 'A'},
	{"discover",       1, 0, 'D'},
	{"preload",        2, 0, 'P'},
	{"scanmem",        0, 0, 'S'},
	{"pre-cmd",        1, 0,  0 },
	{"glc",            2, 0,  0 },
	{0, 0, 0, 0}
};

void use_libmemhack (struct app_options *opt)
{
	if (sizeof(long) == 8)
		opt->preload_lib = (char *) LHACK_PRE "64" LIB_END;
	else if (sizeof(long) == 4)
		opt->preload_lib = (char *) LHACK_PRE "32" LIB_END;
}

void do_assumptions (struct app_options *opt)
{
	/* '-D 1 -A' --> '-D 1', '-S -A' --> '-S'  */
	if ((opt->disc_str && opt->disc_str[0] != '5') || opt->run_scanmem)
		opt->do_adapt = false;
	/* '-A' --> '-A -D 5' */
	if (opt->do_adapt) {
		if (!opt->disc_str)
			opt->disc_str = (char *) "5";
		/* '-P libmemhack*' -> '' */
		if (opt->preload_lib && strncmp(opt->preload_lib, LHACK_PRE,
		    sizeof(LHACK_PRE) - 1) == 0)
			opt->preload_lib = NULL;
	}
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
	opt->do_adapt = false;
	opt->preload_lib = NULL;
	opt->disc_str = NULL;
	opt->run_scanmem = false;
	opt->pre_cmd = NULL;
	opt->use_glc = false;
	/* no direct CLI input */
	opt->home = NULL;
	opt->proc_name = NULL;
	opt->need_shell = false;
	opt->game_call = NULL;
	opt->game_path = NULL;
	opt->game_binpath = NULL;
	opt->game_params = NULL;
	opt->adp_script = NULL;
	opt->adp_required = false;
	opt->adp_req_line = 0;
	opt->use_gbt = false;
	opt->disc_offs = 0;
	opt->disc_addr = NULL;
	opt->scanmem_pid = -1;
}

/*
 * parses the command-line options
 */
char *parse_options (i32 argc, char **argv, struct app_options *opt)
{
	i32 ch = '\0', prev_ch = '\0', opt_idx = 0;

	init_options(opt);

	while (true) {
		prev_ch = ch;
		ch = getopt_long (argc, argv, short_options,
				  long_options, &opt_idx);
		if (ch < 0)
			break;

		switch (ch) {
		case 0:
			if (strncmp(long_options[opt_idx].name,
			    "glc", 3) == 0) {
				if (optind == argc || !optarg)
					opt->pre_cmd = "";
				else
					opt->pre_cmd = optarg;
				opt->use_glc = true;
				opt->need_shell = true;
			} else if (strncmp(long_options[opt_idx].name,
			    "pre-cmd", sizeof("pre-cmd") - 1) == 0) {
				opt->pre_cmd = optarg;
				opt->need_shell = true;
			}
			break;
		case 'h':
			usage();
			break;
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
		default:
			if (optind != argc) {
				if (prev_ch == 'P')
					opt->preload_lib = optarg;
				else
					usage();
			}
			break;
		}
	}
	do_assumptions(opt);

	return argv[optind - 1];
}
