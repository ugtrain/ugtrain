/* cfgparser.h:    parsing functions to read in the config file
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

#ifndef CFGPARSER_H
#define CFGPARSER_H

#include <list>
#include "common.h"
#include "options.h"
#include "cfgentry.h"

void read_config (struct app_options *opt,
		  list<CfgEntry> *cfg,
		  list<CfgEntry*> *cfg_act,
		  list<CfgEntry*> *cfgp_map[],
		  vector<string> *lines);

void write_config_vect (char *path, vector<string> *lines);
#endif
