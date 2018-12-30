/* adapt.h:    adapt the config by running a script
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

#ifndef ADAPT_H
#define ADAPT_H

#include <list>

// local includes
#include <cfgentry.h>
#include <common.h>
#include <options.h>

i32 process_adaptation (Options *opt, vector<string> *cfg_lines);

#endif
