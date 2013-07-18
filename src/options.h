/* options.h:     functions and structs for option handling and help
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

#ifndef OPTIONS_H
#define OPTIONS_H

#include <getopt.h>

#define PROG_NAME  "ugtrain"
#define PRELOADER  "ugpreload"

struct app_options {
	int do_adapt;
	char *preload_lib;
	char *disc_str;
	/* no direct CLI input */
	char *home;
	char *proc_name;
	char *adp_script;
	int adp_required;
	unsigned int adp_req_line;
	void *disc_addr;
};

#ifdef __cplusplus
extern "C" {
#endif
	void usage();
	void use_libmemhack (struct app_options *opt);
	void do_assumptions (struct app_options *opt);
	void parse_options (int argc, char **argv, struct app_options *opt);
#ifdef __cplusplus
};
#endif

#endif
