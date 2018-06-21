/* options.cpp:    option parsing, help, usage, etc.
 *
 * Copyright (c) 2012..2018 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <getopt.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

/* local includes */
#include <common.h>
#include <options.h>
#include <testopts.h>
#include <util.h>


#define DYNMEM_FILE "/tmp/memhack_file"

void do_assumptions (Options *opt)
{
	/* Adaptation handling */
	/* '-D 1 -A' --> '-D 1', '-S -A' --> '-S' */
	if (!opt->disc_str->empty() || opt->run_scanmem)
		opt->do_adapt = false;

	/* Preloading/Starting handling */
	if (!opt->preload_lib) {
		/* '-D <str>' --> '-D <str> -P libmemdisc32/64.so' */
		if (!opt->disc_str->empty()) {
			use_libmemdisc(opt);
		/* '-S' --> '-S -P libmemhack32/64.so',
		   '-A' --> '-A -P libmemhack32/64.so',
		   '--pre-cmd=' --> '--pre-cmd= -P libmemhack32/64.so' */
		} else if (opt->run_scanmem || opt->pre_cmd || opt->do_adapt) {
			use_libmemhack(opt);
		}
	/* '-P libmemhack32/64.so -D <str>' -->
	   '-D <str> -P libmemdisc32/64.so' */
	} else if (!opt->disc_str->empty() && strncmp(opt->preload_lib,
	    LHACK_PRE, sizeof(LHACK_PRE) - 1) == 0) {
		use_libmemdisc(opt);
	}
}

static const char Help[] =
PROG_NAME " is the universal elite game trainer\n"
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
"			- starts the game for pure static memory as well\n"
"--discover, -D [str]:	do discovery by sending the string to the discovery\n"
"			library - without \'-P\', \'-P "
				LDISC_PRE "32/64" LIB_END "\'\n"
"			is assumed\n"
"--scanmem, -S:		run \'scanmem\' without/together with \'-D\' in the\n"
"			same process group as the game (avoid root access)\n"
"			- assumes \'-P\' and ignores the config\n"
"--adapt, -A:		run the adaptation script from the config to determine\n"
"			the new malloc code addresses and start the game with\n"
"			the \'-P\' option\n"
"Misc Options\n"
"--pre-cmd=<str>:	together with the assumed \'-P\' it is possible to\n"
"			specify a preloader command\n"
"			- it is only important that it appends to LD_PRELOAD\n"
TESTING_OPT_HELP
"\n"
"Report bugs to and ask at " PACKAGE_BUGREPORT " for more help!\n"
;

static void usage()
{
	fprintf(stderr, "%s", Help);
	exit(-1);
}

/* use non-printable starting at > 8-bit char table */
enum {
	PreCmd = 300,
	TESTING_OPT_CHARS
};

static const char short_options[] = "-hVAD::P::S";
static struct option long_options[] = {
	{"help",           0, 0, 'h'},
	{"version",        0, 0, 'V'},
	{"adapt",          0, 0, 'A'},
	{"discover",       2, 0, 'D'},
	{"preload",        2, 0, 'P'},
	{"scanmem",        0, 0, 'S'},
	{"pre-cmd",        1, 0, PreCmd },
	TESTING_OPT_LONG
	{0, 0, 0, 0}
};

void cleanup_options (Options *opt)
{
	list<CacheEntry>::iterator cait;
	list<LibEntry>::iterator lit;

	// Free cache lists first
	if (!opt->lib_list)
		goto skip_lib_list;
	list_for_each (opt->lib_list, lit) {
		list_for_each (lit->cache_list, cait)
			delete[] cait->data;
		lit->cache_list->clear();
		delete lit->cache_list;
	}
	opt->lib_list->clear();
	delete opt->lib_list;
skip_lib_list:
	if (!opt->stack || !opt->stack->cache_list)
		goto skip_stack_cache;
	list_for_each (opt->stack->cache_list, cait)
		delete[] cait->data;
	opt->stack->cache_list->clear();
	delete opt->stack->cache_list;
	delete opt->stack;
skip_stack_cache:
	if (!opt->cache_list)
		goto skip_statmem_cache;
	list_for_each (opt->cache_list, cait)
		delete[] cait->data;
	opt->cache_list->clear();
	delete opt->cache_list;

skip_statmem_cache:
	if (opt->cfg_path)
		delete[] opt->cfg_path;
	if (opt->dynmem_file)
		delete[] opt->dynmem_file;
	if (opt->game_path) {
		free(opt->game_path);
		opt->game_path = NULL;
	}
	if (opt->game_binpath)
		delete[] opt->game_binpath;
	if (opt->game_call)
		delete[] opt->game_call;
	if (opt->game_params)
		delete[] opt->game_params;
	if (opt->proc_name)
		delete[] opt->proc_name;
	if (opt->adp_script)
		delete[] opt->adp_script;
	if (opt->disc_lib)
		delete opt->disc_lib;
	if (opt->disc_str)
		delete opt->disc_str;
}

static void init_options (Options *opt)
{
	string tmp_str;

	memset(opt, 0, sizeof(Options));

	/* no direct CLI input */
	init_opt_globals(opt);
	opt->procmem_fd = -1;
	opt->scanmem_pid = -1;
	opt->disc_str = new string;
	opt->disc_lib = new string;
	tmp_str = DYNMEM_FILE;
	opt->dynmem_file = to_c_str(&tmp_str);
	opt->cache_list = new list<CacheEntry>;
	opt->stack = new StackOpt;
	opt->stack->cache_list = new list<CacheEntry>;
	opt->lib_list = new list<LibEntry>;
}

/*
 * parses the command-line options
 */
void parse_options (i32 argc, char **argv, Options *opt)
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
			if (optind == argc || !optarg)
				*opt->disc_str = "2";
			else
				*opt->disc_str = optarg;
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
		TESTING_OPT_PARSING
		default:  /* unknown option */
			if (optind != argc) {
				/* optional argument handling */
				switch (prev_ch) {
				case 'D':
					*opt->disc_str = argv[optind - 1];
					break;
				case 'P':
					opt->preload_lib = argv[optind - 1];
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
