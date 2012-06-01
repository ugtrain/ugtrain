/* parser.cpp:    parsing functions to read in the config file
 *
 * Copyright (c) 2012, by:      Sebastian Riemer
 *    All rights reserved.      Ernst-Reinke-Str. 23
 *                              10369 Berlin, Germany
 *                             <sebastian.riemer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <stdlib.h>
#include "parser.h"
using namespace std;

#define CFG_DIR "."

static inline void proc_name_err (string *line, int lidx)
{
	cerr << "First line doesn't contain a valid process name!" << endl;
	cerr << string(*line, 0, lidx) << "<--" << endl;
	exit(-1);
}

static inline void cfg_parse_err (string *line, int lnr, int lidx)
{
	cerr << "Error while parsing config (line " << ++lnr << ")!" << endl;
	cerr << string(*line, 0, lidx) << "<--" << endl;
	exit(-1);
}

static void read_config_vect (string *path, vector<string> *lines)
{
	ifstream cfg_file;
	string line;

	cout << "Loading config file \"" << *path << "\"." << endl;
	cfg_file.open(path->c_str());
	if (!cfg_file.is_open()) {
		cerr << "File \"" << *path << "\" doesn't exist!" << endl;
		exit(-1);
	}
	while (cfg_file.good()) {
		getline(cfg_file, line);
		lines->push_back(line);
	}
	cfg_file.close();
}

static string parse_proc_name (string *line, int *start)
{
	int lidx;

	if (line->length() == 0)
		proc_name_err(line, 0);

	for (lidx = *start; lidx < line->length(); lidx++) {
		if (!isalpha(line->at(lidx)) && line->at(lidx) != '.')
			proc_name_err(line, lidx);
	}
	*start = lidx;
	return *line;
}

static string parse_value_name (string *line, int lnr, int *start,
				bool *is_check)
{
	int lidx;
	string ret;

	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ' ') {
			break;
		} else if (!isalpha(line->at(lidx))) {
			cfg_parse_err(line, lnr, lidx);
		}
	}

	*start = lidx + 1;
	ret = string(*line, 0, lidx);
	if (ret == "check")
		*is_check = true;
	else
		*is_check = false;

	return ret;
}

static long parse_address (string *line, int lnr, int *start)
{
	int lidx;
	long ret = 0;

	lidx = *start;
	if (lidx + 2 > line->length() || line->at(lidx) != '0' ||
	    line->at(lidx + 1) != 'x')
		cfg_parse_err(line, lnr, --lidx);
	*start = lidx + 2;
	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ' ') {
			ret = strtol(string(*line, *start,
				lidx - *start).c_str(), NULL, 16);
			break;
		} else if (!isxdigit(line->at(lidx))) {
			cfg_parse_err(line, lnr, lidx);
		}
	}
	*start = lidx + 1;
	return ret;
}

static int parse_data_type (string *line, int lnr, int *start, bool *is_signed)
{
	int lidx, ret;

	lidx = *start;
	if (lidx + 2 > line->length())
		cfg_parse_err(line, lnr, --lidx);
	switch (line->at(lidx)) {
	case 'u':
		*is_signed = false;
		break;
	case 'i':
		*is_signed = true;
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
			break;
		} else if (!isdigit(line->at(lidx))) {
			cfg_parse_err(line, lnr, lidx);
		}
	}
	*start = ++lidx;
	return ret;
}

static long parse_value (string *line, int lnr, int *start,
				bool is_signed, int *check)
{
	int lidx;
	long ret;

	lidx = *start;
	if (lidx + 2 > line->length())
		cfg_parse_err(line, lnr, --lidx);
	if (line->at(lidx) == 'l' && line->at(lidx + 1) == ' ') {
		*check = DO_LT;
		*start += 2;
	} else if (line->at(lidx) == 'g' && line->at(lidx + 1) == ' ') {
		*check = DO_GT;
		*start +=2;
	} else {
		*check = DO_UNCHECKED;
	}

	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ' ') {
			break;
		} else if (!isdigit(line->at(lidx)) &&
			   line->at(*start) != '-') {
			cfg_parse_err(line, lnr, lidx);
		}
	}
	if (is_signed)
		ret = strtol(string(*line,
		  *start, lidx - *start).c_str(), NULL, 10);
	else
		ret = strtoul(string(*line,
		  *start, lidx - *start).c_str(), NULL, 10);
	*start = ++lidx;
	return ret;
}

static void parse_key_bindings (string *line, int lnr, int *start,
				       list<CfgEntry> *cfg,
				       list<CfgEntry*> **cfgp_map)
{
	int lidx;
	char key;

	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ',' || line->at(lidx) == ' ') {
			if (lidx == *start + 1) {
				key = line->at(*start);
				*start = lidx + 1;
				if (!cfgp_map[key])
					cfgp_map[key] = new list<CfgEntry*>();
				cfgp_map[key]->push_back(&cfg->back());
			} else {
				cfg_parse_err(line, lnr, lidx);
			}
			if (line->at(lidx) == ' ')
				break;
		} else if (!isdigit(line->at(lidx)) &&
			   !isalpha(line->at(lidx))) {
			cfg_parse_err(line, lnr, lidx);
		}
	}
}

list<CfgEntry*> *read_config (char *cfg_name,
			      string *proc_name,
			      list<CfgEntry> *cfg,
			      list<CfgEntry*> **cfgp_map)
{
	CfgEntry cfg_en;
	CfgEntry *cfg_enp;
	list<CfgEntry*> *cfg_act = new list<CfgEntry*>();
	CheckEntry chk_en;
	list<CheckEntry> *chk_lp;
	int lnr, start = 0;
	bool is_check;
	string line;
	vector<string> lines;
	string path(string(CFG_DIR) + string("/")
		+ string(cfg_name) + string(".conf"));

	// read config into string vector
	read_config_vect(&path, &lines);

	// parse config
	*proc_name = parse_proc_name(&lines.at(0), &start);

	for (lnr = 1; lnr < lines.size(); lnr++) {
		line = lines.at(lnr);
		start = 0;
		if (line.length() <= 0 || line[0] == '#')
			continue;

		cfg_en.name = parse_value_name(&line, lnr, &start, &is_check);
		if (!is_check) {
			cfg_en.checks = NULL;
			cfg_en.addr = parse_address(&line, lnr, &start);
			cfg_en.size = parse_data_type(&line, lnr, &start,
				&cfg_en.is_signed);
			cfg_en.value = parse_value(&line, lnr, &start,
				cfg_en.is_signed, &cfg_en.check);

			cfg->push_back(cfg_en);

			parse_key_bindings(&line, lnr, &start, cfg, cfgp_map);

			// get activation state
			if (start > line.length())
				cfg_parse_err(&line, lnr, --start);
			else if (line.at(start) == 'a')
				cfg_act->push_back(&cfg->back());
		} else {
			cfg_enp = &cfg->back();
			if (!cfg_enp->checks)
				cfg_enp->checks = new list<CheckEntry>();

			chk_lp = cfg_enp->checks;
			chk_en.addr = parse_address(&line, lnr, &start);
			chk_en.size = parse_data_type(&line, lnr, &start,
				&chk_en.is_signed);
			chk_en.value = parse_value(&line, lnr, &start,
				chk_en.is_signed, &chk_en.check);

			chk_lp->push_back(chk_en);
		}
	}

	return cfg_act;
}
