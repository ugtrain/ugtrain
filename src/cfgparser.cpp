/* cfgparser.cpp:    parsing functions to read in the config file
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

#include <fstream>
#include <map>
#include <vector>
#include <cstring>
#include <cstdlib>

// local includes
#include <cfgparser.h>
#include <common.h>
using namespace std;

#define CFG_DIR "~/.ugtrain/"
#define CFG_EXT ".conf"


typedef enum {
	NAME_GAME_PATH,
	NAME_GAME_BINPATH,
	NAME_GAME_CALL,
	NAME_GAME_PARAMS,
	NAME_USE_GBT,
	NAME_NO_PRELOAD,
	NAME_REGULAR,
	NAME_CHECK,
	NAME_CHECK_OBJ,
	NAME_DEFINE,
	NAME_DYNMEM_START,
	NAME_DYNMEM_GROW,
	NAME_DYNMEM_END,
	NAME_DYNMEM_FILE,
	NAME_PTRMEM_START,
	NAME_PTRMEM_END,
	NAME_ADP_SCRIPT,
	NAME_ADP_REQ
} name_e;

#define ASSIGN_CACHE(mem_type, entry, data_type)		\
do {								\
	entry = (data_type) ait->element;			\
	entry->cache = &ait->mem_type->cache_list->back();	\
	entry->cache_data = PTR_ADD(void *, entry->cache->data,	\
		(entry->addr - entry->cache->offs));		\
} while (0)

#define BUILD_CACHE(mem_type)						\
do {									\
	offs_min = ait->mem_type->cache_list->back().offs + MEM_CHUNK;	\
	if (ait->addr + ait->size > offs_min)				\
		offs_min = ait->addr;					\
	if (ait->addr >= offs_min) {					\
		/* init and add a new cache entry */			\
		init_cache(&cache, ait->addr);				\
		ait->mem_type->cache_list->push_back(cache);		\
	}								\
	if (!ait->is_check)						\
		ASSIGN_CACHE(mem_type, cfg_en, CfgEntry *);		\
	else								\
		ASSIGN_CACHE(mem_type, chk_en, CheckEntry *);		\
} while (0)

static inline void
init_cache (CacheEntry *cache, ptr_t addr)
{
	cache->offs = addr;
	cache->start = PTR_MAX;
	cache->data = new char[MEM_CHUNK];
	cache->is_dirty = false;
}

/*
 * temporary list entry for sorting CfgEntry and ChkEntry
 * elements by address for caching
 */
class AddrEntry {
public:
	ptr_t addr;
	size_t size;		// in bytes
	Options *opt;
	DynMemEntry *dynmem;
	PtrMemEntry *ptrmem;
	void *element;		// CfgEntry* or ChkEntry*
	bool is_check;          // type of void pointer
	bool on_stack;		// whether this is a stack address
};

static bool compare_addr_in_list (const AddrEntry first, const AddrEntry second)
{
	return (first.addr < second.addr);
}

#define LIST_ADD_ADDR(it, is_check)			\
do {							\
	addr_en.addr = it->addr;			\
	addr_en.size = it->type.size / 8;		\
	addr_en.on_stack = it->type.on_stack;		\
	addr_en.opt = opt;				\
	addr_en.dynmem = dynmem;			\
	addr_en.ptrmem = ptrmem;			\
	addr_en.element = (void *) &(*it);		\
	addr_en.is_check = is_check;			\
	addrlist->push_back(addr_en);			\
} while (0)

/*
 * Put all dynmem config/check entry addresses in config into a
 * temporary list and sort them. Then allocate and assign caches.
 */
static void
build_caches (Options *opt, list<CfgEntry> *cfg)
{
	AddrEntry addr_en;
	list<AddrEntry> __addrlist, *addrlist = &__addrlist;
	list<AddrEntry>::iterator ait;
	list<CfgEntry>::iterator cfgit;
	list<CheckEntry>::iterator chkit;
	CacheEntry cache;
	ptr_t offs_min;
	bool is_check;

	// build up address list
	list_for_each (cfg, cfgit) {
		DynMemEntry *dynmem = cfgit->dynmem;
		PtrMemEntry *ptrmem = cfgit->ptrmem;
		is_check = false;
		LIST_ADD_ADDR(cfgit, is_check);

		// put checks into address list
		if (!cfgit->checks)
			continue;
		list_for_each (cfgit->checks, chkit) {
			if (chkit->cfg_ref)
				continue;
			is_check = true;
			LIST_ADD_ADDR(chkit, is_check);
		}
	}

	// sort address list
	addrlist->sort(compare_addr_in_list);

	// build and assign caches
	list_for_each (addrlist, ait) {
		CfgEntry *cfg_en;
		CheckEntry *chk_en;

		if (ait->dynmem)
			BUILD_CACHE(dynmem);
		else if (ait->ptrmem)
			BUILD_CACHE(ptrmem);
		else if (ait->on_stack)
			BUILD_CACHE(opt->stack);
		else
			BUILD_CACHE(opt);
	}
}

static PtrMemEntry *find_ptr_mem (list<CfgEntry> *cfg, string *name)
{
	list<CfgEntry>::iterator it;
	PtrMemEntry *old_ptrmem = NULL;

	list_for_each (cfg, it) {
		if (it->ptrmem && it->ptrmem != old_ptrmem) {
			old_ptrmem = it->ptrmem;
			if (it->ptrmem->name == *name)
				return it->ptrmem;
		}
	}
	return NULL;
}

static CfgEntry *find_cfg_en (list<CfgEntry> *cfg, string *name)
{
	list<CfgEntry>::reverse_iterator rit;
	CfgEntry *cfg_en = NULL;

	list_for_each_prev (cfg, rit) {
		cfg_en = &(*rit);
		if (cfg_en->name == *name)
			return cfg_en;
	}
	return NULL;
}

static inline void proc_name_err (string *line, u32 lidx)
{
	cerr << "First line doesn't contain a valid process name!" << endl;
	cerr << string(*line, 0, lidx) << "<--" << endl;
	exit(-1);
}

#define cfg_parse_err(line, lnr, lidx) _cfg_parse_err(line, lnr, lidx, __FILE__, __LINE__)
static inline void _cfg_parse_err (string *line, u32 lnr, u32 lidx, const char *cfile, i32 cline)
{
	cerr << "Error while parsing config (line " << ++lnr
	     << ", " << cfile << ":" << cline << ")!" << endl;
	cerr << string(*line, 0, lidx) << "<--" << endl;
	exit(-1);
}

static char *parse_proc_name (string *line, u32 *start)
{
	u32 lidx;
	char *pname = NULL;

	if (line->length() == 0)
		proc_name_err(line, 0);

	for (lidx = *start; lidx < line->length(); lidx++) {
		if (!isalnum(line->at(lidx)) &&
		    line->at(lidx) != '.' &&
		    line->at(lidx) != '-' &&
		    line->at(lidx) != '_' &&
		    line->at(lidx) != ' ')
			proc_name_err(line, lidx);
	}
	*start = lidx;

	// copy process name as we need it as C string
	pname = to_c_str(line);

	return pname;
}

static map<string,string> macros;

static string parse_value_name (string *line, u32 lnr, u32 *start,
				bool allow_spaces, name_e *name_type)
{
	u32 lidx;
	string ret;

start:
	for (lidx = *start; lidx < line->length(); lidx++) {
		if (!allow_spaces && line->at(lidx) == ' ') {
			break;
		} else if (!isalnum(line->at(lidx))) {
			switch (line->at(lidx)) {
			case '-':
			case '_':
			case '/':
			case '.':
			case ' ':
				break;
			default:
				cfg_parse_err(line, lnr, lidx);
				break;
			}
		}
	}

	ret = string(*line, *start, lidx - *start);
	*start = lidx + 1;
	if (!name_type)
		return ret;

	if (ret.substr(0, 6) == "dynmem") {
		if (ret.substr(6, string::npos) == "start")
			*name_type = NAME_DYNMEM_START;
		else if (ret.substr(6, string::npos) == "grow")
			*name_type = NAME_DYNMEM_GROW;
		else if (ret.substr(6, string::npos) == "end")
			*name_type = NAME_DYNMEM_END;
		else if (ret.substr(6, string::npos) == "file")
			*name_type = NAME_DYNMEM_FILE;
		else
			*name_type = NAME_REGULAR;
	} else if (ret.substr(0, 6) == "ptrmem") {
		if (ret.substr(6, string::npos) == "start")
			*name_type = NAME_PTRMEM_START;
		else if (ret.substr(6, string::npos) == "end")
			*name_type = NAME_PTRMEM_END;
		else
			*name_type = NAME_REGULAR;
	} else if (ret.substr(0, 5) == "check") {
		if (ret.size() == 5)
			*name_type = NAME_CHECK;
		else if (ret.substr(5, string::npos) == "o")
			*name_type = NAME_CHECK_OBJ;
		else
			*name_type = NAME_REGULAR;
	} else if (ret.substr(0, 6) == "adapt_") {
		if (ret.substr(6, string::npos) == "script")
			*name_type = NAME_ADP_SCRIPT;
		else if (ret.substr(6, string::npos) == "required")
			*name_type = NAME_ADP_REQ;
		else
			*name_type = NAME_REGULAR;
	} else if (ret.substr(0, 5) == "game_") {
		if (ret.substr(5, string::npos) == "path")
			*name_type = NAME_GAME_PATH;
		else if (ret.substr(5, string::npos) == "binpath")
			*name_type = NAME_GAME_BINPATH;
		else if (ret.substr(5, string::npos) == "call")
			*name_type = NAME_GAME_CALL;
		else if (ret.substr(5, string::npos) == "params")
			*name_type = NAME_GAME_PARAMS;
		else
			*name_type = NAME_REGULAR;
	} else if (ret == "use_gbt") {
		*name_type = NAME_USE_GBT;
	} else if (ret == "no_preload") {
		*name_type = NAME_NO_PRELOAD;
	} else if (ret == "define") {
		*name_type = NAME_DEFINE;
	} else {
		map<string,string>::iterator it;

		for (it = macros.begin(); it != macros.end(); ++it) {
			if (ret == it->first) {
				*line = it->second;
				*start = 0;
				goto start;
			}
		}
		*name_type = NAME_REGULAR;
	}

	return ret;
}

static bool parse_obj_num_chk (list<CfgEntry> *cfg, CheckEntry *chk_en,
			       string *line, u32 lnr, u32 *start)
{
	u32 lidx;
	bool ret = false;

	lidx = *start;
	if (lidx + 4 > line->length())
		cfg_parse_err(line, lnr, --lidx);
	if (line->at(lidx) == 'o' && line->at(lidx + 1) == 'b' &&
	    line->at(lidx + 2) == 'j' && line->at(lidx + 3) == ' ') {
		ret = true;
		*start = lidx + 4;
	}
	return ret;
}

static inline ptr_t get_cfg_en_addr (CheckEntry *chk_en, CfgEntry *cfg_en)
{
	if (cfg_en->type.on_stack)
		chk_en->type.on_stack = true;
	return cfg_en->addr;
}

static ptr_t parse_address (list<CfgEntry> *cfg, CfgEntry *cfg_en,
			    CheckEntry *chk_en, string *line, u32 lnr,
			    u32 *start)
{
	u32 lidx;
	string tmp_str;
	ptr_t ret = 0;
	bool use_refaddr = false;

start:
	lidx = *start;
	if (lidx + 2 > line->length())
		cfg_parse_err(line, lnr, --lidx);
	if (line->at(lidx) != '0' || line->at(lidx + 1) != 'x') {
		// So you want a special address or a cfg value reference?
		tmp_str = parse_value_name(line, lnr, start, false, NULL);
		// Use the config reference address instead of its value?
		if (tmp_str == "addr") {
			if (use_refaddr || !chk_en)
				cfg_parse_err(line, lnr, --lidx);
			use_refaddr = true;
			goto start;
		}
		if (tmp_str == "stack") {
			if (!cfg_en)
				cfg_parse_err(line, lnr, lidx);
			if (chk_en)
				chk_en->type.on_stack = true;
			else
				cfg_en->type.on_stack = true;
			cfg = NULL; cfg_en = NULL; chk_en = NULL;
			goto start;
		}
		if (!cfg || !chk_en || !cfg_en)
			cfg_parse_err(line, lnr, lidx);
		if (tmp_str == "this") {
			ret = get_cfg_en_addr(chk_en, cfg_en);
			goto out;
		}
		chk_en->cfg_ref = find_cfg_en(cfg, &tmp_str);
		if (!chk_en->cfg_ref)
			cfg_parse_err(line, lnr, lidx);
		// Checks are processed before the current cfg entry.
		// So take over its address instead.
		if (chk_en->cfg_ref == cfg_en) {
			ret = get_cfg_en_addr(chk_en, cfg_en);
			chk_en->cfg_ref = NULL;
		} else if (use_refaddr) {
			ret = get_cfg_en_addr(chk_en, chk_en->cfg_ref);
			chk_en->cfg_ref = NULL;
		}
		goto out;
	}
	*start = lidx + 2;
	for (lidx = *start; lidx < line->length(); lidx++) {
		if (lidx == line->length() - 1) {
			ret = strtoptr(string(*line, *start,
				lidx + 1 - *start).c_str(), NULL, 16);
			break;
		} else if (line->at(lidx) == ' ') {
			ret = strtoptr(string(*line, *start,
				lidx - *start).c_str(), NULL, 16);
			break;
		} else if (!isxdigit(line->at(lidx))) {
			cfg_parse_err(line, lnr, lidx);
		}
	}
	*start = lidx + 1;
out:
	return ret;
}

static void parse_data_type (string *line, u32 lnr, u32 *start,
			     struct type *type)
{
	u32 lidx;

	type->size = 32;
	lidx = *start;
	if (lidx + 2 > line->length())
		cfg_parse_err(line, lnr, --lidx);
	switch (line->at(lidx)) {
	case 'u':
		type->is_signed = false;
		type->is_float = false;
		break;
	case 'i':
		type->is_signed = true;
		type->is_float = false;
		break;
	case 'f':
		type->is_signed = true;
		type->is_float = true;
		break;
	case 'p':
		lidx += 2;
		*start = lidx;
		type->size = 0;
		return;
	case 'c':
		if (lidx + 8 > line->length())
			cfg_parse_err(line, lnr, --lidx);
		if (line->substr(lidx, 5) == "cstrp") {
			lidx += 6;
			*start = lidx;
			type->is_cstrp = true;
			type->size = 0;
		} else {
			cfg_parse_err(line, lnr, --lidx);
		}
		return;
	default:
		cfg_parse_err(line, lnr, --lidx);
		break;
	}

	*start = ++lidx;
	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ' ') {
			type->size = atoi(string(*line,
			  *start, lidx - *start).c_str());
			if (type->is_float &&
			    (type->size != 32 && type->size != 64))
				cfg_parse_err(line, lnr, lidx);
			break;
		} else if (!isdigit(line->at(lidx))) {
			cfg_parse_err(line, lnr, lidx);
		}
	}
	*start = ++lidx;
}

/*
 * This function parses a signed/unsigned integer or a float/double value
 * from the config and also determines if a check is wanted. Dynamic wish
 * values are also possible.
 */
static value_t parse_value (string *line, u32 lnr, u32 *start,
			    list<CfgEntry> *cfg, CfgEntry *cfg_en, CheckEntry *chk_en,
			    dynval_e *dynval, check_e *check, u32 i)
{
	u32 lidx, number_base = 10;
	value_t ret;
	i64 tmp_i64;
	u64 tmp_u64;
	bool dynval_detected = false;
	string tmp_str;
	struct type __type, *type = &__type;

	/* init */
	__type.is_float = false;
	__type.is_signed = false;
	__type.size = 32;
	ret.i64 = 0;

	if (cfg_en)
		type = &cfg_en->type;
	else if (chk_en)
		type = &chk_en->type;

	lidx = *start;
	if (!check)
		goto skip_check;

	// determine check
	if (lidx + 2 > line->length()) {
		if (i == 0)
			cfg_parse_err(line, lnr, --lidx);
		*check = CHECK_END;
		return ret;
	}
	if (line->at(lidx) == 'l' && line->at(lidx + 1) == ' ') {
		*check = CHECK_LT;
		*start += 2;
	} else if (line->at(lidx) == 'g' && line->at(lidx + 1) == ' ') {
		*check = CHECK_GT;
		*start += 2;
	} else if (line->at(lidx) == 'e' && line->at(lidx + 1) == ' ') {
		*check = CHECK_EQ;
		*start += 2;
		if (!chk_en)
			cfg_parse_err(line, lnr, lidx);
		tmp_str = string(*line, *start, lidx - *start);
		if (&chk_en->check[0] == check && tmp_str == "heap") {
			chk_en->is_heapchk = true;
			lidx = line->length();
			goto out;
		}
	} else {
		if (!dynval)    // This must be a check entry.
			cfg_parse_err(line, lnr, --lidx);
		*check = CHECK_OFF;
	}

skip_check:
	// Is this a hex value?
	tmp_str = string(*line, *start, line->length() - *start);
	if (tmp_str.substr(0, 2) == "0x") {
		*start += 2;
		number_base = 16;
	}
	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ' ') {
			break;
		} else if (number_base == 10 &&
		    !isdigit(line->at(lidx)) && line->at(*start) != '-' &&
		    !(type->is_float && line->at(lidx) == '.')) {
			if (!dynval || !isalpha(line->at(lidx)))
				cfg_parse_err(line, lnr, lidx);
			else
				dynval_detected = true;
		} else if (number_base == 16 && !isxdigit(line->at(lidx))) {
			cfg_parse_err(line, lnr, lidx);
		}
	}
	if (dynval_detected) {
		tmp_str = string(*line, *start, lidx - *start);
		if (tmp_str == "max") {
			*dynval = DYN_VAL_MAX;
		} else if (tmp_str == "min") {
			*dynval = DYN_VAL_MIN;
		} else if (tmp_str.substr(0, 4) == "va0x") {
			*dynval = DYN_VAL_ADDR;
			if (sscanf(tmp_str.substr(2).c_str(), SCN_PTR, &ret.ptr) != 1)
				cfg_parse_err(line, lnr, lidx);
		} else if (tmp_str == "watch") {
			*dynval = DYN_VAL_WATCH;
		} else if (tmp_str == "once") {
			*dynval = DYN_VAL_PTR_ONCE;
		} else if (tmp_str == "always") {
			*dynval = DYN_VAL_PTR_ALWAYS;
		} else {
			if (!cfg || !cfg_en)
				cfg_parse_err(line, lnr, lidx);
			// So you want a cfg value reference?
			cfg_en->cfg_ref = find_cfg_en(cfg, &tmp_str);
			if (!cfg_en->cfg_ref)
				cfg_parse_err(line, lnr, lidx);
			*dynval = DYN_VAL_ADDR;
		}
		goto out;
	}

	if (type->is_float) {
		if (type->size == 32)
			ret.f32 = strtof(string(*line, *start,
				lidx - *start).c_str(), NULL);
		else
			ret.f64 = strtod(string(*line, *start,
				lidx - *start).c_str(), NULL);
	} else if (type->is_signed) {
		tmp_i64 = strtoll(string(*line,
			*start, lidx - *start).c_str(), NULL, number_base);
		switch (type->size) {
		case 64:
			ret.i64 = tmp_i64;
			break;
		case 32:
			ret.i32 = (i32) tmp_i64;
			break;
		case 16:
			ret.i16 = (i16) tmp_i64;
			break;
		default:
			ret.i8 = (i8) tmp_i64;
			break;
		}
	} else {
		tmp_u64 = strtoull(string(*line,
			*start, lidx - *start).c_str(), NULL, number_base);
		switch (type->size) {
		case 64:
			ret.u64 = tmp_u64;
			break;
		case 32:
			ret.u32 = (u32) tmp_u64;
			break;
		case 16:
			ret.u16 = (u16) tmp_u64;
			break;
		default:
			ret.u8 = (u8) tmp_u64;
			break;
		}
	}
out:
	*start = ++lidx;
	return ret;
}

static u32 parse_u32_value (string *line, u32 lnr, u32 *start)
{
	value_t value = parse_value(line, lnr, start, NULL, NULL, NULL, NULL, NULL, 0);

	return value.u32;
}

static void parse_key_bindings (string *line, u32 lnr, u32 *start,
				list<CfgEntry> *cfg,
				list<CfgEntry*> *cfgp_map[])
{
	u32 lidx;
	char ch;
	i32 ch_idx;

	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ',' || line->at(lidx) == ' ') {
			if (lidx == *start + 1) {
				ch = line->at(*start);
				ch_idx = (i32) ch;
				*start = lidx + 1;
				if (!cfgp_map[ch_idx])
					cfgp_map[ch_idx] = new list<CfgEntry*>();
				cfgp_map[ch_idx]->push_back(&cfg->back());
			} else {
				cfg_parse_err(line, lnr, lidx);
			}
			if (line->at(lidx) == ' ')
				break;
		} else if (!isalnum(line->at(lidx))) {
			cfg_parse_err(line, lnr, lidx);
		}
	}
}

/*
 * returns the string at the end of a line
 * returns NULL if there is no string, it is "exe" or contains blanks
 */
static inline char *parse_pic_lib (string *line, u32 *start)
{
	string tmp_str;
	char *lib = NULL;

	if (*start >= line->length())
		goto out;

	tmp_str = line->substr(*start);
	if (tmp_str == "exe" || tmp_str.find(' ') != string::npos ||
	    tmp_str.find('\t') != string::npos)
		goto out;
	lib = to_c_str(&tmp_str);
out:
	return lib;
}

static void parse_growing (DynMemEntry *dynmem_enp, string *line, u32 lnr,
			   u32 *start)
{
	GrowEntry *grow_enp = new GrowEntry();
	u32 lidx = *start;

	// get minimum and maximum size
	grow_enp->size_min = parse_u32_value(line, lnr, start);
	if (grow_enp->size_min == 0)
		cfg_parse_err(line, lnr, --lidx);
	grow_enp->size_max = parse_u32_value(line, lnr, start);
	if (grow_enp->size_max <= grow_enp->size_min)
		cfg_parse_err(line, lnr, --lidx);

	// parse type of growing
	lidx = *start;
	if (lidx + 2 > line->length())
		cfg_parse_err(line, lnr, --lidx);
	if (line->at(lidx) == '+') {
		grow_enp->type = GROW_ADD;
		*start = ++lidx;
		grow_enp->add = parse_u32_value(line, lnr, start);
		if (grow_enp->add == 0 ||
		    grow_enp->add > grow_enp->size_max -
		    grow_enp->size_min)
			cfg_parse_err(line, lnr, --lidx);
	} else {
		cfg_parse_err(line, lnr, --lidx);
	}
	// parse backtracing
	grow_enp->code_addr =
		parse_address(NULL, NULL, NULL, line, lnr, start);
	grow_enp->code_offs = grow_enp->code_addr;
	grow_enp->stack_offs =
		parse_address(NULL, NULL, NULL, line, lnr, start);
	grow_enp->lib = parse_pic_lib(line, start);
	grow_enp->v_msize.clear();
	dynmem_enp->grow = grow_enp;
}

static void parse_dynmem (DynMemEntry *dynmem_enp, bool from_grow, string *line,
			  u32 lnr, u32 *start)
{
	CacheEntry cache;
	dynmem_enp->name = parse_value_name(line, lnr, start, false, NULL);
	if (from_grow) {
		dynmem_enp->mem_size = 0;
		dynmem_enp->code_addr = 0;
		dynmem_enp->code_offs = 0;
		dynmem_enp->stack_offs = 0;
		dynmem_enp->cfg_line = 0;
		dynmem_enp->lib = NULL;
	} else {
		dynmem_enp->mem_size = parse_u32_value(line, lnr, start);
		dynmem_enp->code_addr =
			parse_address(NULL, NULL, NULL, line, lnr, start);
		dynmem_enp->code_offs = dynmem_enp->code_addr;
		dynmem_enp->stack_offs =
			parse_address(NULL, NULL, NULL, line, lnr, start);
		dynmem_enp->cfg_line = lnr;
		dynmem_enp->lib = parse_pic_lib(line, start);
	}
	init_cache(&cache, PTR_MAX);
	dynmem_enp->cache_list = new list<CacheEntry>;
	dynmem_enp->cache_list->push_back(cache);
	dynmem_enp->grow = NULL;
	dynmem_enp->v_maddr.clear();
}

static inline void pr_warn_statmem_ptr (void)
{
	cout << "Pointer following from static memory is experimental."
	     << endl;
}

static void read_config_vect (string *path, char *home, vector<string> *lines)
{
	ifstream cfg_file;
	string line;

	if (path->rfind('~', 0) != string::npos)
		path->replace(0, 1, home);

	if (path->find(CFG_EXT, path->size() - sizeof(CFG_EXT) + 1) == string::npos)
		*path += string(CFG_EXT);

	cfg_file.open(path->c_str());
	if (!cfg_file.is_open()) {
		*path = string(CFG_DIR) + *path;
		if (path->rfind('~', 0) != string::npos)
			path->replace(0, 1, home);

		cfg_file.open(path->c_str());
		if (!cfg_file.is_open()) {
			cerr << "File \"" << *path << "\" doesn't exist!" << endl;
			exit(-1);
		}
	}
	cout << "Loading config file \"" << *path << "\"." << endl;
	while (cfg_file.good()) {
		getline(cfg_file, line);
		lines->push_back(line);
	}
	cfg_file.close();
}

#define FIND_CACHE_START_CHECK(mem_type)				\
do {									\
	if (!chk_en.cfg_ref &&						\
	    chk_en.addr < mem_type->cache_list->back().offs)		\
		mem_type->cache_list->back().offs = chk_en.addr;	\
} while (0)

#define FIND_CACHE_START(mem_type)					\
do {									\
	if (cfg_en.addr < mem_type->cache_list->back().offs)		\
		mem_type->cache_list->back().offs = cfg_en.addr;	\
} while (0)

void read_config (Options *opt,
		  list<CfgEntry> *cfg,
		  list<CfgEntry*> *cfg_act,
		  list<CfgEntry*> *cfgp_map[],
		  vector<string> *lines)
{
	string tmp_str, key, line;
	CfgEntry cfg_en;
	CfgEntry *cfg_enp;
	list<CfgEntry*> *used_cfg_act = NULL;
	CheckEntry chk_en;
	list<CheckEntry> *chk_lp;
	DynMemEntry *dynmem_enp = NULL;
	PtrMemEntry *ptrmem_enp = NULL;
	u32 i, lnr, start = 0;
	name_e name_type;
	bool in_dynmem = false, in_ptrmem = false;
	size_t pos;
	CacheEntry cache_en;

	// init static memory cache
	init_cache(&cache_en, PTR_MAX);
	opt->cache_list->push_back(cache_en);

	// init stack memory cache
	init_cache(&cache_en, PTR_MAX);
	opt->stack->cache_list->push_back(cache_en);

	// read config into string vector
	tmp_str = string(opt->cfg_path);
	read_config_vect(&tmp_str, opt->home, lines);
	opt->cfg_path = to_c_str(&tmp_str);

	// parse config
	opt->proc_name = parse_proc_name(&lines->at(0), &start);
	opt->game_call = opt->proc_name;

	for (lnr = 1; lnr < lines->size(); lnr++) {
		line = lines->at(lnr);
		start = 0;
		if (line.length() <= 0 || line[0] == '#')
			continue;

		cfg_en.name = parse_value_name(&line, lnr, &start, false,
			&name_type);
		switch (name_type) {
		case NAME_CHECK_OBJ:
			if (!in_dynmem)
				cfg_parse_err(&line, lnr, start);
			// fall through
		case NAME_CHECK:
			if (name_type == NAME_CHECK_OBJ)
				chk_en.is_objcheck = true;
			else
				chk_en.is_objcheck = false;
			chk_en.is_heapchk = false;
			cfg_enp = &cfg->back();
			if (!cfg_enp->checks)
				cfg_enp->checks = new list<CheckEntry>();

			chk_lp = cfg_enp->checks;
			chk_en.cfg_ref = NULL;
			chk_en.cache = NULL;
			chk_en.cache_data = NULL;
			memset(&chk_en.type, 0, sizeof(chk_en.type));
			if (in_dynmem)
				chk_en.check_obj_num = parse_obj_num_chk(cfg,
					&chk_en, &line, lnr, &start);
			else
				chk_en.check_obj_num = false;
			if (!chk_en.check_obj_num)
				chk_en.addr = parse_address(cfg, cfg_enp, &chk_en,
							    &line, lnr, &start);
			if (chk_en.type.on_stack) {
				if (in_dynmem || in_ptrmem)
					cfg_parse_err(&line, lnr, start);
				opt->val_on_stack = true;
			}
			parse_data_type(&line, lnr, &start, &chk_en.type);
			if (!chk_en.type.size)
				chk_en.type.size = sizeof(long) * 8;
			for (i = 0; i < MAX_CHK_VALS; i++)
				chk_en.value[i] = parse_value(&line, lnr, &start, cfg,
					NULL, &chk_en, NULL, &chk_en.check[i], i);
			if (chk_en.is_heapchk) {
				opt->heap_checks = true;
				chk_en.check[1] = CHECK_GT;
			}
			chk_en.check[MAX_CHK_VALS] = CHECK_END;
			if (in_dynmem)
				FIND_CACHE_START_CHECK(cfg_enp->dynmem);
			else if (in_ptrmem)
				FIND_CACHE_START_CHECK(cfg_enp->ptrmem);
			else if (chk_en.type.on_stack)
				FIND_CACHE_START_CHECK(opt->stack);
			else
				FIND_CACHE_START_CHECK(opt);
			chk_lp->push_back(chk_en);
			if (chk_en.is_heapchk) {
				chk_en.check[0] = CHECK_LT;
				chk_en.check[1] = CHECK_END;
				chk_lp->push_back(chk_en);
			}
			break;

		case NAME_DYNMEM_START:
			if (in_ptrmem || in_dynmem)
				cfg_parse_err(&line, lnr, start);
			in_dynmem = true;
			dynmem_enp = new DynMemEntry();
			parse_dynmem(dynmem_enp, false, &line, lnr, &start);
			break;

		case NAME_DYNMEM_GROW:
			if (in_ptrmem)
				cfg_parse_err(&line, lnr, start);
			if (!in_dynmem) {
				in_dynmem = true;
				dynmem_enp = new DynMemEntry();
				parse_dynmem(dynmem_enp, true, &line, lnr, &start);
			}
			if (!dynmem_enp || dynmem_enp->grow)
				cfg_parse_err(&line, lnr, start);
			parse_growing(dynmem_enp, &line, lnr, &start);
			break;

		case NAME_DYNMEM_END:
			if (in_dynmem) {
				in_dynmem = false;
				// leaking dynmem_enp intentionally
				dynmem_enp = NULL;
			} else {
				cfg_parse_err(&line, lnr, start);
			}
			break;

		case NAME_DYNMEM_FILE:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			tmp_str = parse_value_name(&line, lnr,
				&start, true, NULL);
			opt->dynmem_file = to_c_str(&tmp_str);
			break;

		case NAME_PTRMEM_START:
			CacheEntry cache;
			if (in_dynmem)
				cfg_parse_err(&line, lnr, start);
			in_ptrmem = true;
			ptrmem_enp = new PtrMemEntry();
			ptrmem_enp->name = parse_value_name(&line, lnr,
				&start, false, NULL);
			ptrmem_enp->mem_size = parse_u32_value(&line, lnr,
				&start);
			init_cache(&cache, PTR_MAX);
			ptrmem_enp->cache_list = new list<CacheEntry>;
			ptrmem_enp->cache_list->push_back(cache);
			break;

		case NAME_PTRMEM_END:
			if (in_ptrmem) {
				in_ptrmem = false;
				ptrmem_enp = NULL;
			} else {
				cfg_parse_err(&line, lnr, start);
			}
			break;

		case NAME_ADP_SCRIPT:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			tmp_str = string(opt->cfg_path);
			pos = tmp_str.rfind("/");
			if (pos != string::npos)
				tmp_str.erase(pos + 1, string::npos);
			else
				tmp_str.clear();
			tmp_str += parse_value_name(&line, lnr, &start,
				false, NULL);

			// Copy into C string
			opt->adp_script = to_c_str(&tmp_str);

			// check for added invalid parameters
			if (start < line.length() - 1)
				cfg_parse_err(&line, lnr, start);
			break;

		case NAME_ADP_REQ:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			if (parse_u32_value(&line, lnr, &start))
				opt->adp_required = true;
			opt->adp_req_line = lnr;
			break;

		case NAME_GAME_PATH:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			tmp_str = parse_value_name(&line,
				  lnr, &start, true, NULL);

			pos = tmp_str.rfind("/");
			if (pos != string::npos &&
			    tmp_str.substr(pos + 1, string::npos) !=
			    opt->game_call)
				cfg_parse_err(&line, lnr, start);

			// Copy into C string
			opt->game_path = to_c_str(&tmp_str);
			break;

		case NAME_GAME_CALL:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			tmp_str = parse_value_name(&line,
				  lnr, &start, false, NULL);

			// Copy into C string
			opt->game_call = to_c_str(&tmp_str);
			break;

		case NAME_GAME_BINPATH:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			tmp_str = parse_value_name(&line,
				  lnr, &start, true, NULL);

			pos = tmp_str.rfind("/");
			if (pos != string::npos &&
			    tmp_str.substr(pos + 1, string::npos) !=
			    opt->proc_name)
				cfg_parse_err(&line, lnr, start);

			// Copy into C string
			opt->game_binpath = to_c_str(&tmp_str);
			opt->binpath_line = lnr;
			break;

		case NAME_GAME_PARAMS:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			tmp_str = line.substr(start, string::npos);

			// Copy into C string
			opt->game_params = to_c_str(&tmp_str);
			break;

		case NAME_USE_GBT:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			opt->use_gbt = true;
			break;

		case NAME_NO_PRELOAD:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			if (opt->preload_lib)
				opt->preload_lib = (char*) "-";
			break;

		case NAME_DEFINE:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			key = parse_value_name(&line, lnr,
				&start, false, NULL);
			tmp_str = line.substr(start, string::npos);
			macros[key] = tmp_str;
			break;

		default:
			cfg_en.checks = NULL;
			cfg_en.dynval = DYN_VAL_OFF;
			cfg_en.cfg_ref = NULL;
			cfg_en.cache = NULL;
			cfg_en.cache_data = NULL;
			memset(&cfg_en.type, 0, sizeof(cfg_en.type));
			cfg_en.addr = parse_address(cfg, &cfg_en, NULL, &line, lnr, &start);
			if (cfg_en.type.on_stack) {
				if (in_dynmem || in_ptrmem)
					cfg_parse_err(&line, lnr, start);
				opt->val_on_stack = true;
			}
			parse_data_type(&line, lnr, &start, &cfg_en.type);
			cfg_en.ptrtgt = NULL;
			cfg_en.cstr = NULL;
			if (cfg_en.type.is_cstrp) {
				cfg_en.type.size = sizeof(long) * 8;
				if (!in_dynmem && !in_ptrmem)
					cfg_en.cstr = (char *) calloc(1, MAX_CSTR + 1);
			} else if (!cfg_en.type.size) {
				tmp_str = parse_value_name(&line, lnr, &start,
					false, NULL);
				cfg_en.ptrtgt = find_ptr_mem(cfg, &tmp_str);
				if (!cfg_en.ptrtgt)
					cfg_parse_err(&line, lnr, start - 1);
				if (in_dynmem) {
					cfg_en.ptrtgt->dynmem = dynmem_enp;
				} else {
					pr_warn_statmem_ptr();
					cfg_en.ptrtgt->v_state.push_back(PTR_INIT);
					cfg_en.ptrtgt->v_offs.push_back(0);
				}
				cfg_en.type.size = sizeof(long) * 8;
			}

			cfg_en.value = parse_value(&line, lnr, &start, cfg, &cfg_en,
				NULL, &cfg_en.dynval, &cfg_en.check, 0);
			// Cannot allow other than watching for C/C++ strings
			if (cfg_en.type.is_cstrp && cfg_en.dynval != DYN_VAL_WATCH)
				cfg_parse_err(&line, lnr, --start);
			if (cfg_en.dynval == DYN_VAL_ADDR)
				cfg_en.val_addr = cfg_en.value.ptr;
			if (in_dynmem) {
				FIND_CACHE_START(dynmem_enp);
				cfg_en.dynmem = dynmem_enp;
			} else {
				cfg_en.dynmem = NULL;
			}

			cfg->push_back(cfg_en);

			if (in_ptrmem) {
				FIND_CACHE_START(ptrmem_enp);
				cfg->back().ptrmem = ptrmem_enp;
				cfg->back().ptrmem->cfg.push_back(&cfg->back());
				used_cfg_act = &cfg->back().ptrmem->cfg_act;
			} else {
				if (cfg->back().type.on_stack)
					FIND_CACHE_START(opt->stack);
				else if (!in_dynmem)
					FIND_CACHE_START(opt);
				cfg->back().ptrmem = NULL;
				used_cfg_act = cfg_act;
			}

			if (cfg->back().dynval == DYN_VAL_WATCH ||
			    cfg->back().ptrtgt) {
				used_cfg_act->push_back(&cfg->back());
				if (in_dynmem)
					cfg->back().dynmem->cfg_act.push_back(&cfg->back());
				break;
			}

			parse_key_bindings(&line, lnr, &start, cfg, cfgp_map);

			// get activation state
			if (start > line.length()) {
				cfg_parse_err(&line, lnr, --start);
			} else if (line.at(start) == 'a') {
				used_cfg_act->push_back(&cfg->back());
				if (in_dynmem)
					cfg->back().dynmem->cfg_act.push_back(&cfg->back());
			} else if (line.at(start) == 'w') {
				cfg->back().dynval = DYN_VAL_WATCH;
				used_cfg_act->push_back(&cfg->back());
				if (in_dynmem)
					cfg->back().dynmem->cfg_act.push_back(&cfg->back());
			}
			break;
		}
	}
	macros.clear();

	// End of config checks
	if (dynmem_enp) {
		delete dynmem_enp;
		dynmem_enp = NULL;
		cfg_parse_err(&line, --lnr, start);
	}
	if (in_dynmem || in_ptrmem)
		cfg_parse_err(&line, --lnr, start);

	build_caches(opt, cfg);
}
