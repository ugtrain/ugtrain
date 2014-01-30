/* valoutput.h:    print values read from victim process memory
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

#ifndef VALOUTPUT_H
#define VALOUTPUT_H

#include "common.h"
#include "cfgentry.h"

void output_mem_val (CfgEntry *cfg_en, void *mem_offs, bool is_dynmem);
void output_ptrmem_values (CfgEntry *cfg_en);

#endif
