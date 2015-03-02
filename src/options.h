/* options.h:     functions and structs for option handling and help
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
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

/* local includes */
#include <common.h>
#include <testopts.h>

#define PROG_NAME  "ugtrain"
#define GLC_PRELOADER "glc-capture"
#define SCANMEM    "scanmem"

#define LDISC_PRE "libmemdisc"
#define LHACK_PRE "libmemhack"
#define LIB_END   ".so"

struct app_options {
	char	*cfg_path;
	char	*pre_cmd;
	char	*preload_lib;
	char	*disc_str;
	char	*disc_lib;
	bool	do_adapt;
	bool	run_scanmem;
	bool	use_glc;
	/* no direct CLI input */
	bool	pure_statmem;
	bool	have_objdump;
	bool	use_gbt;
	bool	adp_required;
	u32	adp_req_line;
	char	*adp_script;
	char	*home;
	char	*proc_name;
	char	*game_call;
	char	*game_path;
	char	*game_binpath;
	u32     binpath_line;
	char	*game_params;
	char	*dynmem_file;
	ptr_t	disc_addr;
	ptr_t	code_addr;
	ptr_t	code_offs;
	size_t	disc_offs;
	pid_t	scanmem_pid;
	/* options for testing */
	TESTING_OPT_VARS
};

#ifdef __cplusplus
extern "C" {
#endif
	void do_assumptions (struct app_options *opt);
	void parse_options (i32 argc, char **argv, struct app_options *opt);

	/* inline functions */
	static inline void use_libmemdisc (struct app_options *opt)
	{
		if (sizeof(ptr_t) == 8)
			opt->preload_lib = (char *) LDISC_PRE "64" LIB_END;
		else if (sizeof(ptr_t) == 4)
			opt->preload_lib = (char *) LDISC_PRE "32" LIB_END;
	}

	static inline void use_libmemhack (struct app_options *opt)
	{
		if (sizeof(ptr_t) == 8)
			opt->preload_lib = (char *) LHACK_PRE "64" LIB_END;
		else if (sizeof(ptr_t) == 4)
			opt->preload_lib = (char *) LHACK_PRE "32" LIB_END;
	}
#ifdef __cplusplus
};
#endif

#endif
