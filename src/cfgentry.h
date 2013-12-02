/* cfgentry.h:     classes for config read from a file
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

#ifndef CFGENTRY_H
#define CFGENTRY_H

#include <vector>
#include <list>
#include "common.h"


class CfgEntry;

class DynMemEntry {
public:
	string name;
	size_t mem_size;
	void *code_addr;
	void *stack_offs;

	// later determined values
	vector<void *> v_maddr;    // set by malloc calls
	u32 num_alloc;   // how many obj. created at once
	u32 num_freed;   // how many obj. freed at once
	u32 obj_idx;     // object index for object check
	u32 pr_idx;      // print index

	// adaption
	void *adp_addr;               // adapted code address
	void *adp_soffs;              // adapted reverse stack offset
	u32 cfg_line;                 // to write back new cfg
};

typedef enum {
	PTR_INIT,
	PTR_SETTLED,
	PTR_DONE,
} ptr_e;

class PtrMemEntry {
public:
	string name;
	size_t mem_size;

	list<CfgEntry *> cfg;
	list<CfgEntry *> cfg_act;

	DynMemEntry *dynmem;
	vector<ptr_e> v_state;
	vector<void *> v_offs;
};


typedef enum {
	DYN_VAL_OFF,
	DYN_VAL_MIN,
	DYN_VAL_MAX,
	DYN_VAL_ADDR,
	DYN_VAL_WATCH,
	DYN_VAL_PTR_ONCE,
	DYN_VAL_PTR_ALWAYS,
} dynval_e;

typedef enum {
	CHECK_OFF,
	CHECK_LT,
	CHECK_GT,
	CHECK_EQ,
	CHECK_END,
} check_e;

#define MAX_CHK_VALS 4  // for "or" checks

class CheckEntry {
public:
	void *addr;
	CfgEntry *cfg_ref;
	bool is_objcheck;
	bool is_signed;
	bool is_float;
	i32 size;
	check_e check[MAX_CHK_VALS + 1];
	i64 value[MAX_CHK_VALS + 1];
};

class CfgEntry {
public:
	string name;
	void *addr;
	bool is_signed;
	bool is_float;
	i32 size;
	check_e check;
	dynval_e dynval;
	void *val_addr;   // for DYN_VAL_ADDR only
	i64 value;
	i64 old_val;      // for static memory only

	// get value from other cfg entry (DYN_VAL_ADDR)
	CfgEntry *cfg_ref;
	// more checks
	list<CheckEntry> *checks;
	// dynamic memory
	DynMemEntry *dynmem;
	vector<i64> v_oldval;   // old value per object
	// pointer memory
	PtrMemEntry *ptrmem;
	PtrMemEntry *ptrtgt;
};

#endif
