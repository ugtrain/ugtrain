/* cfgoutput.h:    output functions for the ugtrain config
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

#ifndef CFGOUTPUT_H
#define CFGOUTPUT_H

#include <list>
#include "common.h"
#include "cfgentry.h"

void output_config (list<CfgEntry> *cfg);
void output_configp (list<CfgEntry*> *cfg);

#endif
