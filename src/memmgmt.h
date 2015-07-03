/* memmgmt.h:    allocation and freeing of objects and values
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * powered by the Open Game Cheating Association
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MEMMGMT_H
#define MEMMGMT_H

#include <list>

// local includes
#include <cfgentry.h>
#include <common.h>
#include <fifoparser.h>

// malloc queue to delay malloc processing
struct mqueue {
	char    *data;
	ssize_t size;
	ssize_t end;
};

void queue_dynmem_addr (MF_PARAMS);
void alloc_dynmem_addr (MF_PARAMS);
void clear_dynmem_addr (FF_PARAMS);
int  output_dynmem_changes (list<CfgEntry> *cfg);
void free_dynmem  (list<CfgEntry> *cfg, bool process_kicked);
void alloc_dynmem (list<CfgEntry> *cfg);

#endif
