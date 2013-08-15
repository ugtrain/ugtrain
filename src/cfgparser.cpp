/* cfgparser.cpp:    parsing functions to read in the config file
 *
 * Copyright (c) 2012..13, by:  Sebastian Riemer
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

#include <fstream>
#include <vector>
#include <cstring>
#include <stdlib.h>
#include "common.h"
#include "cfgparser.h"
using namespace std;


#define CFG_DIR "~/.ugtrain/"
#define CFG_EXT ".conf"

enum {
	NAME_REGULAR,
	NAME_CHECK,
	NAME_DYNMEM_START,
	NAME_DYNMEM_END,
	NAME_DYNMEM_STACK,
	NAME_ADAPT,
	NAME_ADP_REQ
};

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
				i32 *name_type)
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
	if (ret.substr(0, 6) == "dynmem") {
		if (ret.substr(6, string::npos) == "start")
			*name_type = NAME_DYNMEM_START;
		else if (ret.substr(6, string::npos) == "end")
			*name_type = NAME_DYNMEM_END;
		else if (ret.substr(6, string::npos) == "stack")
			*name_type = NAME_DYNMEM_STACK;
		else
			*name_type = NAME_REGULAR;
	} else {
		if (ret == "check")
			*name_type = NAME_CHECK;
		else if (ret == "adapt_script")
			*name_type = NAME_ADAPT;
		else if (ret == "adapt_required")
			*name_type = NAME_ADP_REQ;
		else
			*name_type = NAME_REGULAR;
	}

	return ret;
}

static void *parse_address (string *line, u32 lnr, u32 *start)
{
	u32 lidx;
	void *ret = NULL;

	lidx = *start;
	if (lidx + 2 > line->length() || line->at(lidx) != '0' ||
	    line->at(lidx + 1) != 'x')
		cfg_parse_err(line, lnr, --lidx);
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
 * from the config and also determines if a greater/less than check is wanted.
 *
 * Attention: Hacky floats. A float has 4 bytes, a double has 8 bytes
 *            and i64 has 8 bytes. Why not copy the float/double into the i64?!
 *            We always parse floats as doubles here.
 */
static i64 parse_value (string *line, u32 lnr, u32 *start, bool is_signed,
		        bool is_float, i32 *dynval, i32 *check)
{
	u32 lidx;
	i64 ret = 0;
	double tmp_dval;
	bool dynval_detected = false;
	string tmp_str;

	lidx = *start;
	if (!check)
		goto skip_check;

	// determine check
	if (lidx + 2 > line->length())
		cfg_parse_err(line, lnr, --lidx);
	if (line->at(lidx) == 'l' && line->at(lidx + 1) == ' ') {
		*check = DO_LT;
		*start += 2;
	} else if (line->at(lidx) == 'g' && line->at(lidx + 1) == ' ') {
		*check = DO_GT;
		*start += 2;
	} else {
		*check = DO_UNCHECKED;
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
		} else {
			cfg_parse_err(line, lnr, lidx);
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
				list<CfgEntry*> **cfgp_map)
{
	u32 lidx;
	char key;

	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ',' || line->at(lidx) == ' ') {
			if (lidx == *start + 1) {
				key = line->at(*start);
				*start = lidx + 1;
				if (!cfgp_map[(i32)key])
					cfgp_map[(i32)key] = new list<CfgEntry*>();
				cfgp_map[(i32)key]->push_back(&cfg->back());
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
			      list<CfgEntry*> **cfgp_map,
			      vector<string> *lines)
{
	CfgEntry cfg_en;
	CfgEntry *cfg_enp;
	list<CfgEntry*> *cfg_act = new list<CfgEntry*>();
	CheckEntry chk_en;
	list<CheckEntry> *chk_lp;
	DynMemEntry *dynmem_enp = NULL;
	u32 lnr, start = 0;
	i32 name_type;
	bool in_dynmem = false;
	string line;
	string home(opt->home);
	string adp_str;
	size_t pos;

	// read config into string vector
	read_config_vect(path, &home, lines);

	// parse config
	opt->proc_name = parse_proc_name(&lines->at(0), &start);

	for (lnr = 1; lnr < lines->size(); lnr++) {
		line = lines->at(lnr);
		start = 0;
		if (line.length() <= 0 || line[0] == '#')
			continue;

		cfg_en.name = parse_value_name(&line, lnr, &start, &name_type);
		switch (name_type) {
		case NAME_CHECK:
			cfg_enp = &cfg->back();
			if (!cfg_enp->checks)
				cfg_enp->checks = new list<CheckEntry>();

			chk_lp = cfg_enp->checks;
			chk_en.addr = parse_address(&line, lnr, &start);
			chk_en.size = parse_data_type(&line, lnr, &start,
				&chk_en.is_signed, &chk_en.is_float);
			chk_en.value = parse_value(&line, lnr, &start, chk_en.is_signed,
				chk_en.is_float, NULL, &chk_en.check);

			chk_lp->push_back(chk_en);
			break;

		case NAME_DYNMEM_START:
			in_dynmem = true;
			dynmem_enp = new DynMemEntry();
			dynmem_enp->name = parse_value_name(&line, lnr,
				&start, &name_type);
			dynmem_enp->mem_size = parse_value(&line, lnr,
				&start, false, false, NULL, NULL);
			dynmem_enp->code_addr = parse_address(&line, lnr, &start);
			dynmem_enp->stack_offs[dynmem_enp->num_stack] =
				parse_address(&line, lnr, &start);
			dynmem_enp->v_maddr.clear();
			dynmem_enp->cfg_lines[dynmem_enp->num_stack] = lnr;
			dynmem_enp->num_stack++;
			break;

		case NAME_DYNMEM_END:
			if (in_dynmem) {
				in_dynmem = false;
				dynmem_enp = NULL;
			} else {
				cfg_parse_err(&line, lnr, start);
			}
			break;

		case NAME_DYNMEM_STACK:
			if (in_dynmem) {
				if (dynmem_enp->num_stack >= MAX_STACK)
					cfg_parse_err(&line, lnr, start);
				dynmem_enp->stack_offs[dynmem_enp->num_stack] =
					parse_address(&line, lnr, &start);
				dynmem_enp->cfg_lines[dynmem_enp->num_stack] = lnr;
				dynmem_enp->num_stack++;
			} else {
				cfg_parse_err(&line, lnr, start);
			}
			break;

		case NAME_ADAPT:
			if (in_dynmem)
				cfg_parse_err(&line, lnr, start);

			adp_str = string("");
			pos = path->rfind("/");
			if (pos != string::npos)
				adp_str.append(path->substr(0, pos + 1));
			adp_str.append(parse_value_name(&line,
				       lnr, &start, &name_type));

			// Copy into C string
			opt->adp_script = to_c_str(&adp_str);
			break;

		case NAME_ADP_REQ:
			if (in_dynmem)
				cfg_parse_err(&line, lnr, start);

			opt->adp_required = parse_value(&line, lnr, &start,
							false, false, NULL, NULL);
			opt->adp_req_line = lnr;
			break;

		default:
			cfg_en.checks = NULL;
			cfg_en.dynval = DYN_VAL_NONE;
			cfg_en.addr = parse_address(&line, lnr, &start);
			cfg_en.size = parse_data_type(&line, lnr, &start,
				&cfg_en.is_signed, &cfg_en.is_float);
			cfg_en.value = parse_value(&line, lnr, &start, cfg_en.is_signed,
				cfg_en.is_float, &cfg_en.dynval, &cfg_en.check);
			if (cfg_en.dynval == DYN_VAL_ADDR)
				cfg_en.val_addr = (void *) cfg_en.value;
			if (in_dynmem) {
				cfg_en.dynmem = dynmem_enp;
			} else {
				cfg_en.dynmem = NULL;
			}

			cfg->push_back(cfg_en);

			parse_key_bindings(&line, lnr, &start, cfg, cfgp_map);

			// get activation state
			if (start > line.length())
				cfg_parse_err(&line, lnr, --start);
			else if (line.at(start) == 'a')
				cfg_act->push_back(&cfg->back());
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
	for (it = lines->begin(); it != lines->end(); ++it)
		cfg_file << (*it) << endl;

	cfg_file.close();
}
