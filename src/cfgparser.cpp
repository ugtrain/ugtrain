/* cfgparser.cpp:    parsing functions to read in the config file
 *
 * Copyright (c) 2012..13, by:  Sebastian Riemer
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

#include <fstream>
#include <vector>
#include <cstring>
#include <stdlib.h>
#include "common.h"
#include "cfgparser.h"
using namespace std;


#define CFG_DIR "~/.ugtrain/"
#define CFG_EXT ".conf"

typedef enum {
	NAME_GAME_PATH,
	NAME_GAME_CALL,
	NAME_GAME_PARAMS,
	NAME_USE_GBT,
	NAME_REGULAR,
	NAME_CHECK,
	NAME_CHECK_OBJ,
	NAME_DYNMEM_START,
	NAME_DYNMEM_END,
	NAME_PTRMEM_START,
	NAME_PTRMEM_END,
	NAME_ADAPT,
	NAME_ADP_REQ
} name_e;

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
	list<CfgEntry>::iterator it;
	CfgEntry *cfg_en = NULL;


	list_for_each (cfg, it) {
		cfg_en = &(*it);
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

static inline void cfg_parse_err (string *line, u32 lnr, u32 lidx)
{
	cerr << "Error while parsing config (line " << ++lnr << ")!" << endl;
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
		    line->at(lidx) != '_')
			proc_name_err(line, lidx);
	}
	*start = lidx;

	// copy process name as we need it as C string
	pname = to_c_str(line);

	return pname;
}

static string parse_value_name (string *line, u32 lnr, u32 *start,
				name_e *name_type)
{
	u32 lidx;
	string ret;

	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ' ') {
			break;
		} else if (!isalnum(line->at(lidx)) &&
		    line->at(lidx) != '-' &&
		    line->at(lidx) != '_' &&
		    line->at(lidx) != '/' &&
		    line->at(lidx) != '.') {
			cfg_parse_err(line, lnr, lidx);
		}
	}

	ret = string(*line, *start, lidx - *start);
	*start = lidx + 1;
	if (!name_type)
		return ret;

	if (ret.substr(0, 6) == "dynmem") {
		if (ret.substr(6, string::npos) == "start")
			*name_type = NAME_DYNMEM_START;
		else if (ret.substr(6, string::npos) == "end")
			*name_type = NAME_DYNMEM_END;
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
			*name_type = NAME_ADAPT;
		else if (ret.substr(6, string::npos) == "required")
			*name_type = NAME_ADP_REQ;
		else
			*name_type = NAME_REGULAR;
	} else if (ret.substr(0, 5) == "game_") {
		if (ret.substr(5, string::npos) == "path")
			*name_type = NAME_GAME_PATH;
		else if (ret.substr(5, string::npos) == "call")
			*name_type = NAME_GAME_CALL;
		else if (ret.substr(5, string::npos) == "params")
			*name_type = NAME_GAME_PARAMS;
		else
			*name_type = NAME_REGULAR;
	} else if (ret == "use_gbt") {
		*name_type = NAME_USE_GBT;
	} else {
		*name_type = NAME_REGULAR;
	}

	return ret;
}

static void *parse_address (list<CfgEntry> *cfg, CheckEntry *chk_en,
			    string *line, u32 lnr, u32 *start)
{
	u32 lidx;
	string tmp_str;
	void *ret = NULL;

	lidx = *start;
	if (lidx + 2 > line->length())
		cfg_parse_err(line, lnr, --lidx);
	if (line->at(lidx) != '0' || line->at(lidx + 1) != 'x') {
		if (!chk_en)
			cfg_parse_err(line, lnr, lidx);
		// So you want a cfg value reference?
		tmp_str = parse_value_name(line, lnr, start, NULL);
		chk_en->cfg_ref = find_cfg_en(cfg, &tmp_str);
		if (!chk_en->cfg_ref)
			cfg_parse_err(line, lnr, lidx);
		goto out;
	}
	*start = lidx + 2;
	for (lidx = *start; lidx < line->length(); lidx++) {
		if (lidx == line->length() - 1) {
			ret = (void *) strtol(string(*line, *start,
				lidx + 1 - *start).c_str(), NULL, 16);
			break;
		} else if (line->at(lidx) == ' ') {
			ret = (void *) strtol(string(*line, *start,
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

static i32 parse_data_type (string *line, u32 lnr, u32 *start,
			    bool *is_signed, bool *is_float)
{
	u32 lidx;
	i32 ret = 32;

	lidx = *start;
	if (lidx + 2 > line->length())
		cfg_parse_err(line, lnr, --lidx);
	switch (line->at(lidx)) {
	case 'u':
		*is_signed = false;
		*is_float = false;
		break;
	case 'i':
		*is_signed = true;
		*is_float = false;
		break;
	case 'f':
		*is_signed = true;
		*is_float = true;
		break;
	case 'p':
		lidx += 2;
		*start = lidx;
		return 0;
	default:
		cfg_parse_err(line, lnr, --lidx);
		break;
	}

	*start = ++lidx;
	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ' ') {
			ret = atoi(string(*line,
			  *start, lidx - *start).c_str());
			if (*is_float && (ret != 32 && ret != 64))
				cfg_parse_err(line, lnr, lidx);
			break;
		} else if (!isdigit(line->at(lidx))) {
			cfg_parse_err(line, lnr, lidx);
		}
	}
	*start = ++lidx;
	return ret;
}

/*
 * This function parses a signed/unsigned integer or a float/double value
 * from the config and also determines if a check is wanted. Dynamic wish
 * values are also possible.
 *
 * Attention: Hacky floats. A float has 4 bytes, a double has 8 bytes
 *            and i64 has 8 bytes. Why not copy the float/double into the i64?!
 *            We always parse floats as doubles here.
 */
static i64 parse_value (string *line, u32 lnr, u32 *start,
			list<CfgEntry> *cfg, CfgEntry *cfg_en, CheckEntry *chk_en,
			dynval_e *dynval, check_e *check, u32 i)
{
	u32 lidx;
	i64 ret = 0;
	double tmp_dval;
	bool is_signed = false, is_float = false;
	bool dynval_detected = false;
	string tmp_str;

	if (cfg_en) {
		is_signed = cfg_en->is_signed;
		is_float = cfg_en->is_float;
	} else if (chk_en) {
		is_signed = chk_en->is_signed;
		is_float = chk_en->is_float;
	}

	lidx = *start;
	if (!check)
		goto skip_check;

	// determine check
	if (lidx + 2 > line->length()) {
		if (i == 0)
			cfg_parse_err(line, lnr, --lidx);
		*check = CHECK_END;
		return 0;
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
	} else {
		if (!dynval)    // This must be a check entry.
			cfg_parse_err(line, lnr, --lidx);
		*check = CHECK_OFF;
	}

skip_check:
	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ' ') {
			break;
		} else if (!isdigit(line->at(lidx)) && line->at(*start) != '-' &&
		    !(is_float && line->at(lidx) == '.')) {
			if (!dynval || !isalpha(line->at(lidx)))
				cfg_parse_err(line, lnr, lidx);
			else
				dynval_detected = true;
		}
	}
	if (dynval_detected) {
		tmp_str = string(*line, *start, lidx - *start);
		if (tmp_str == "max") {
			*dynval = DYN_VAL_MAX;
		} else if (tmp_str == "min") {
			*dynval = DYN_VAL_MIN;
		} else if (tmp_str.substr(0, 2) == "0x") {
			*dynval = DYN_VAL_ADDR;
			if (sscanf(tmp_str.c_str(), "%p", (void **) &ret) != 1)
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
	if (is_float) {
		tmp_dval = strtod(string(*line, *start,
			lidx - *start).c_str(), NULL);
		memcpy(&ret, &tmp_dval, sizeof(i64));  // hacky double to hex
	} else if (is_signed) {
		ret = strtoll(string(*line,
			*start, lidx - *start).c_str(), NULL, 10);
	} else {
		ret = strtoull(string(*line,
			*start, lidx - *start).c_str(), NULL, 10);
	}
out:
	*start = ++lidx;
	return ret;
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

static void read_config_vect (string *path, string *home, vector<string> *lines)
{
	ifstream cfg_file;
	string line;

	if (path->rfind('~', 0) != string::npos)
		path->replace(0, 1, *home);

	if (path->find(CFG_EXT, path->size() - sizeof(CFG_EXT) + 1) == string::npos)
		*path += string(CFG_EXT);

	cfg_file.open(path->c_str());
	if (!cfg_file.is_open()) {
		*path = string(CFG_DIR) + *path;
		if (path->rfind('~', 0) != string::npos)
			path->replace(0, 1, *home);

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

list<CfgEntry*> *read_config (string *path,
			      struct app_options *opt,
			      list<CfgEntry> *cfg,
			      list<CfgEntry*> *cfgp_map[],
			      vector<string> *lines)
{
	CfgEntry cfg_en;
	CfgEntry *cfg_enp;
	list<CfgEntry*> *cfg_act = new list<CfgEntry*>();
	list<CfgEntry*> *used_cfg_act = NULL;
	CheckEntry chk_en;
	list<CheckEntry> *chk_lp;
	DynMemEntry *dynmem_enp = NULL;
	PtrMemEntry *ptrmem_enp = NULL;
	u32 i, lnr, start = 0;
	name_e name_type;
	bool in_dynmem = false, in_ptrmem = false;
	string line;
	string home(opt->home);
	string tmp_str;
	size_t pos;

	// read config into string vector
	read_config_vect(path, &home, lines);

	// parse config
	opt->proc_name = parse_proc_name(&lines->at(0), &start);
	opt->game_call = opt->proc_name;

	for (lnr = 1; lnr < lines->size(); lnr++) {
		line = lines->at(lnr);
		start = 0;
		if (line.length() <= 0 || line[0] == '#')
			continue;

		cfg_en.name = parse_value_name(&line, lnr, &start, &name_type);
		switch (name_type) {
		case NAME_CHECK_OBJ:
			if (!in_dynmem)
				cfg_parse_err(&line, lnr, start);
		case NAME_CHECK:
			if (name_type == NAME_CHECK_OBJ)
				chk_en.is_objcheck = true;
			else
				chk_en.is_objcheck = false;
			cfg_enp = &cfg->back();
			if (!cfg_enp->checks)
				cfg_enp->checks = new list<CheckEntry>();

			chk_lp = cfg_enp->checks;
			chk_en.cfg_ref = NULL;
			chk_en.addr = parse_address(cfg, &chk_en, &line, lnr, &start);
			chk_en.size = parse_data_type(&line, lnr, &start,
				&chk_en.is_signed, &chk_en.is_float);
			for (i = 0; i < MAX_CHK_VALS; i++)
				chk_en.value[i] = parse_value(&line, lnr, &start, cfg,
					NULL, &chk_en, NULL, &chk_en.check[i], i);
			chk_en.check[MAX_CHK_VALS] = CHECK_END;
			chk_lp->push_back(chk_en);
			break;

		case NAME_DYNMEM_START:
			if (in_ptrmem)
				cfg_parse_err(&line, lnr, start);
			in_dynmem = true;
			dynmem_enp = new DynMemEntry();
			dynmem_enp->name = parse_value_name(&line, lnr,
				&start, NULL);
			dynmem_enp->mem_size = parse_value(&line, lnr,
				&start, NULL, NULL, NULL, NULL, NULL, 0);
			dynmem_enp->code_addr = parse_address(cfg, NULL, &line, lnr, &start);
			dynmem_enp->stack_offs = parse_address(cfg, NULL, &line, lnr, &start);
			dynmem_enp->v_maddr.clear();
			dynmem_enp->cfg_line = lnr;
			break;

		case NAME_DYNMEM_END:
			if (in_dynmem) {
				in_dynmem = false;
				dynmem_enp = NULL;
			} else {
				cfg_parse_err(&line, lnr, start);
			}
			break;

		case NAME_PTRMEM_START:
			if (in_dynmem)
				cfg_parse_err(&line, lnr, start);
			in_ptrmem = true;
			ptrmem_enp = new PtrMemEntry();
			ptrmem_enp->name = parse_value_name(&line, lnr,
				&start, NULL);
			ptrmem_enp->mem_size = parse_value(&line, lnr,
				&start, NULL, NULL, NULL, NULL, NULL, 0);
			break;

		case NAME_PTRMEM_END:
			if (in_ptrmem) {
				in_ptrmem = false;
				ptrmem_enp = NULL;
			} else {
				cfg_parse_err(&line, lnr, start);
			}
			break;

		case NAME_ADAPT:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			tmp_str = string("");
			pos = path->rfind("/");
			if (pos != string::npos)
				tmp_str.append(path->substr(0, pos + 1));
			tmp_str.append(parse_value_name(&line,
				       lnr, &start, NULL));

			// Copy into C string
			opt->adp_script = to_c_str(&tmp_str);
			break;

		case NAME_ADP_REQ:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			if (parse_value(&line, lnr, &start,
					NULL, NULL, NULL, NULL, NULL, 0))
				opt->adp_required = true;
			opt->adp_req_line = lnr;
			break;

		case NAME_GAME_PATH:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			tmp_str = parse_value_name(&line,
				  lnr, &start, NULL);

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
				  lnr, &start, NULL);

			// Copy into C string
			opt->game_call = to_c_str(&tmp_str);
			break;

		case NAME_GAME_PARAMS:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			tmp_str = line.substr(start, string::npos);

			// Copy into C string
			opt->game_params = to_c_str(&tmp_str);
			opt->need_shell = true;
			break;

		case NAME_USE_GBT:
			if (in_dynmem || in_ptrmem)
				cfg_parse_err(&line, lnr, start);

			opt->use_gbt = true;
			break;

		default:
			cfg_en.checks = NULL;
			cfg_en.dynval = DYN_VAL_OFF;
			cfg_en.cfg_ref = NULL;
			cfg_en.addr = parse_address(cfg, NULL, &line, lnr, &start);
			cfg_en.size = parse_data_type(&line, lnr, &start,
				&cfg_en.is_signed, &cfg_en.is_float);
			if (!cfg_en.size) {
				tmp_str = parse_value_name(&line, lnr, &start, NULL);
				cfg_en.ptrtgt = find_ptr_mem(cfg, &tmp_str);
				if (!cfg_en.ptrtgt)
					cfg_parse_err(&line, lnr, start - 1);
				if (in_dynmem)
					cfg_en.ptrtgt->dynmem = dynmem_enp;
				else
					cfg_parse_err(&line, lnr, start);
			} else {
				cfg_en.ptrtgt = NULL;
			}

			cfg_en.value = parse_value(&line, lnr, &start, cfg, &cfg_en,
				NULL, &cfg_en.dynval, &cfg_en.check, 0);
			if (cfg_en.dynval == DYN_VAL_ADDR)
				cfg_en.val_addr = (void *) cfg_en.value;
			if (in_dynmem)
				cfg_en.dynmem = dynmem_enp;
			else
				cfg_en.dynmem = NULL;

			cfg->push_back(cfg_en);

			if (in_ptrmem) {
				cfg->back().ptrmem = ptrmem_enp;
				cfg->back().ptrmem->cfg.push_back(&cfg->back());
				used_cfg_act = &cfg->back().ptrmem->cfg_act;
			} else {
				cfg->back().ptrmem = NULL;
				used_cfg_act = cfg_act;
			}

			if (cfg->back().dynval == DYN_VAL_WATCH ||
			    cfg->back().ptrtgt) {
				used_cfg_act->push_back(&cfg->back());
				break;
			}

			parse_key_bindings(&line, lnr, &start, cfg, cfgp_map);

			// get activation state
			if (start > line.length()) {
				cfg_parse_err(&line, lnr, --start);
			} else if (line.at(start) == 'a') {
				used_cfg_act->push_back(&cfg->back());
			} else if (line.at(start) == 'w') {
				cfg->back().dynval = DYN_VAL_WATCH;
				used_cfg_act->push_back(&cfg->back());
			}
			break;
		}
	}

	return cfg_act;
}

void write_config_vect (string *path, vector<string> *lines)
{
	ofstream cfg_file;
	vector<string>::iterator it;

	lines->pop_back();

	cfg_file.open(path->c_str(), fstream::trunc);
	if (!cfg_file.is_open()) {
		cerr << "File \"" << *path << "\" doesn't exist!" << endl;
		exit(-1);
	}
	vect_for_each (lines, it)
		cfg_file << (*it) << endl;

	cfg_file.close();
}
