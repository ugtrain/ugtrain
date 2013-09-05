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

enum {
	DO_UNCHECKED,
	DO_LT,
	DO_GT
};

enum {
	DYN_VAL_NONE,
	DYN_VAL_MIN,
	DYN_VAL_MAX,
	DYN_VAL_ADDR,
	DYN_VAL_WATCH,
};

class CheckEntry {
public:
	void *addr;
	bool is_signed;
	bool is_float;
	i32 size;
	i64 value;
	i32 check;
	bool is_objcheck;
};

class DynMemEntry {
public:
	string name;
	size_t mem_size;
	void *code_addr;
	void *stack_offs[MAX_STACK];
	u8 num_stack;

	/* later determined values */
	vector<void *> v_maddr;    /* set by malloc calls */
	u32 objidx;
	u32 pridx;

	/* adaption */
	void *adp_addr;               /* adapted code address */
	void *adp_soffs[MAX_STACK];   /* adapted stack offsets */
	bool soffs_ign[MAX_STACK];    /* stack offset for adaption only? */
	u32 cfg_lines[MAX_STACK];     /* to write back new cfg */
	u8 adp_sidx;                  /* index in adapted stack offs */
	bool adp_failed;
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
	i32 dynval;
	void *val_addr;
	i32 check;
	list<CheckEntry> *checks;
	DynMemEntry *dynmem;
	vector<i64> v_oldval;
};

#endif
