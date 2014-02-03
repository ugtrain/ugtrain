/* memmgmt.h:    allocation and freeing of objects and values
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

#ifndef MEMMGMT_H
#define MEMMGMT_H

#include <list>
#include "common.h"
#include "cfgentry.h"

void alloc_dynmem_addr (list<CfgEntry> *cfg, struct post_parse *pp,
			void *heap_start, void *mem_addr, size_t mem_size,
			void *code_addr, void *stack_offs);
void clear_dynmem_addr (list<CfgEntry> *cfg, void *argp, void *mem_addr);
void output_dynmem_changes (list<CfgEntry> *cfg);
void free_dynmem  (list<CfgEntry> *cfg, bool process_kicked);
void alloc_dynmem (list<CfgEntry> *cfg);

#endif
