/* parser.h:    parsing functions to read in the config file
 *
 * Copyright (c) 2012..13, by:  Sebastian Riemer
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

#ifndef PARSER_H
#define PARSER_H

#include <list>
#include <string>
#include "cfgentry.h"

list<CfgEntry*> *read_config (char *cfg_name,
			      char *env_home,
			      char **proc_name,
			      char **adp_script,
			      list<CfgEntry> *cfg,
			      list<CfgEntry*> **cfgp_map);

#endif
