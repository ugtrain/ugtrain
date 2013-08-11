/* cfgentry.h:     classes for config read from a file
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

#ifndef CFGENTRY_H
#define CFGENTRY_H

#include <vector>
#include <list>
#include "common.h"

#define MAX_STACK 4

enum {
	DO_UNCHECKED,
	DO_LT,
	DO_GT
};

class CheckEntry {
public:
	void *addr;
	bool is_signed;
	bool is_float;
	i32 size;
	i64 value;
	i32 check;
};

class DynMemEntry {
public:
	string name;
	size_t mem_size;
	void *code_addr;
	void *stack_offs[MAX_STACK];

	/* later determined values */
	vector<void *> v_maddr;    /* set by malloc calls */

	/* adaption */
	void *adp_addr;    /* adapted code address */
	void *adp_stack;   /* adapted stack offset */
	u32 cfg_line;      /* to write back new cfg */
};

class CfgEntry {
public:
	string name;
	void *addr;
	bool is_signed;
	bool is_float;
	i32 size;
	i64 value;
	i64 old_val;
	i32 check;
	list<CheckEntry> *checks;
	DynMemEntry *dynmem;
};

#endif
