/* options.h:     functions and structs for option handling and help
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

#ifndef OPTIONS_H
#define OPTIONS_H

#include <list>
#include <string>

/* local includes */
#include <common.h>
#include <cfgentry.h>
#include <testopts.h>

#define PROG_NAME  "ugtrain"
#define SCANMEM    "scanmem"

#define LDISC_PRE "libmemdisc"
#define LHACK_PRE "libmemhack"
#define LIB_END   ".so"

// Struct declarations
struct list_head;

class StackOpt {
public:
	list<CacheEntry> *cache_list;  // caching stack memory
};

class Options {
public:
	string	*cfg_path;
	char	*pre_cmd;
	char	*preload_lib;
	string	*disc_str;
	string	*disc_lib;
	bool	do_adapt;
	bool	run_scanmem;
	// no direct CLI input
	i32	procmem_fd;
	bool	pure_statmem;
	bool	have_objdump;
	bool	val_on_stack;
	bool	heap_checks;
	bool	use_gbt;
	bool	adp_required;
	u32	adp_req_line;
	char	*adp_script;
	char	*home;
	string  *proc_name;
	string	*game_call;
	char	*game_path;
	char	*game_binpath;
	u32	binpath_line;
	char	*game_params;
	char	*dynmem_file;
	ptr_t	disc_addr;
	ptr_t	code_addr;
	ptr_t	code_offs;
	ptr_t	stack_start;
	ptr_t	stack_end;
	ptr_t	heap_start;
	ptr_t	heap_end;
	size_t	disc_offs;
	pid_t	scanmem_pid;
	struct list_head	*rlist;
	list<CfgEntry>		*cfg;
	list<CfgEntry*>		*cfg_act;
	list<CfgEntry*>		**cfgp_map;
	list<CacheEntry>	*cache_list;	// caching static memory
	list<LibEntry>		*lib_list;	// handling statmem in libraries
	StackOpt		*stack;
	// options for testing
	TESTING_OPT_VARS
};

void do_assumptions (Options *opt);
void parse_options (i32 argc, char **argv, Options *opt);
void cleanup_options (Options *opt);

/* inline functions */
static inline void use_libmemdisc (Options *opt)
{
	if (sizeof(ptr_t) == 8)
		opt->preload_lib = (char *) LDISC_PRE "64" LIB_END;
	else if (sizeof(ptr_t) == 4)
		opt->preload_lib = (char *) LDISC_PRE "32" LIB_END;
}

static inline void use_libmemhack (Options *opt)
{
	if (sizeof(ptr_t) == 8)
		opt->preload_lib = (char *) LHACK_PRE "64" LIB_END;
	else if (sizeof(ptr_t) == 4)
		opt->preload_lib = (char *) LHACK_PRE "32" LIB_END;
}

#endif
