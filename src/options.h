/* options.h:     functions and structs for option handling and help
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

#ifndef OPTIONS_H
#define OPTIONS_H

#include "common.h"

#define PROG_NAME  "ugtrain"
#define PRELOADER  "ugpreload"
#define GLC_PRELOADER "glc-capture"
#define SCANMEM    "scanmem"

struct app_options {
	char	*cfg_path;
	bool	do_adapt;
	char	*preload_lib;
	char	*disc_str;
	bool	run_scanmem;
	char	*pre_cmd;
	bool	use_glc;
	/* no direct CLI input */
	bool	adp_required;
	u32	adp_req_line;
	char	*adp_script;
	char	*home;
	char	*proc_name;
	bool	need_shell;
	char	*game_call;
	char	*game_path;
	char	*game_binpath;
	char	*game_params;
	bool	use_gbt;
	size_t	disc_offs;
	void	*disc_addr;
	pid_t	scanmem_pid;
};

#ifdef __cplusplus
extern "C" {
#endif
	void usage();
	void use_libmemhack (struct app_options *opt);
	void do_assumptions (struct app_options *opt);
	void parse_options (i32 argc, char **argv, struct app_options *opt);
#ifdef __cplusplus
};
#endif

#endif
