/* util.cpp:    C++ utility functions
 *
 * Copyright (c) 2018 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

// local includes
#include <util.h>


/* Static variables for atexit() handler */
static Options _static_opt = { 0 }, *static_opt = &_static_opt;
static vector<string> _static_cfg_lines, *static_cfg_lines = &_static_cfg_lines;
static list<CfgEntry> _static_cfg, *static_cfg = &_static_cfg;
static list<CfgEntry*> _static_cfg_act, *static_cfg_act = &_static_cfg_act;
static list<CfgEntry*> *static_cfgp_map[CFGP_MAP_SIZE] = { NULL };
static struct dynmem_params _static_dmparams, *static_dmparams = &_static_dmparams;
static LIST_HEAD(_static_rlist);
static struct list_head *static_rlist = &_static_rlist;
static Globals static_globals = {
	static_opt, static_cfg_lines, static_cfg, static_cfg_act, static_cfgp_map,
	static_dmparams, static_rlist
};


static inline void
init_opt_globals (Options *opt)
{
	opt->cfg = static_cfg;
	opt->cfg_act = static_cfg_act;
	opt->cfgp_map = static_cfgp_map;
	opt->rlist = static_rlist;
}

/* Get static variables for atexit() handler */
Globals *get_globals (void)
{
	init_opt_globals(static_opt);
	return &static_globals;
}


#define CLEANUP_MEM(mem_type, code)				\
do {								\
	list<CfgEntry>::iterator cfg_it;			\
	vector<CacheEntry>::iterator cait;			\
								\
	list_for_each (cfg, cfg_it) {				\
		if (cfg_it->mem_type != mem_type)		\
			continue;				\
		cfg_it->mem_type = NULL;			\
	}							\
	vect_for_each (mem_type->cache_vect, cait)		\
		delete[] cait->data;				\
	mem_type->cache_vect->clear();				\
	delete mem_type->cache_vect;				\
	code;							\
	delete mem_type;					\
} while (0)

void clear_config (void)
{
	list<CfgEntry> *cfg = static_cfg;
	list<CfgEntry*> *cfg_act = static_cfg_act;
	list<CfgEntry*> **cfgp_map = static_cfgp_map;
	struct dynmem_params *dmparams = static_dmparams;
	struct list_head *rlist = static_rlist;
	list<CfgEntry>::iterator it;
	i32 ch_idx;

	// Clean up memory regions list and malloc() queue
	rlist_clear(rlist);
	if (dmparams->mqueue && dmparams->mqueue->data) {
		free(dmparams->mqueue->data);
		dmparams->mqueue->data = NULL;
	}

	// Clean up config lines read
	static_cfg_lines->clear();

	// Clean up active config and key map
	cfg_act->clear();
	for (ch_idx = 0; ch_idx < CFGP_MAP_SIZE; ch_idx++) {
		if (!cfgp_map[ch_idx])
			continue;
		cfgp_map[ch_idx]->clear();
		delete cfgp_map[ch_idx];
		cfgp_map[ch_idx] = NULL;
	}
	// Clean up static memory config
	list_for_each (cfg, it) {
		if (it->dynmem || it->ptrmem)
			continue;
		if (it->type.is_cstrp)
			free(it->cstr);
		if (it->type.lib_name)
			delete it->type.lib_name;
	}
	// Clean up dynamic/pointer memory config
	list_for_each (cfg, it) {
		DynMemEntry *dynmem = it->dynmem;
		PtrMemEntry *ptrmem = it->ptrmem;
		if (dynmem)
			CLEANUP_MEM(dynmem,
				if (dynmem->lib) delete dynmem->lib);
		else if (ptrmem)
			CLEANUP_MEM(ptrmem, ;);
		// Clean up checks
		if (it->checks) {
			list<CheckEntry>::iterator chk_it;
			list_for_each (it->checks, chk_it) {
				if (chk_it->type.lib_name)
					delete chk_it->type.lib_name;
			}
			it->checks->clear();
			delete it->checks;
		}
	}
	cfg->clear();
}

/*
 * Make Valgrind happy to avoid false-positive leak reporting.
 * atexit() handler
 */
void cleanup_ugtrain_atexit (void)
{
	clear_config();

	cleanup_options(static_opt);
}
