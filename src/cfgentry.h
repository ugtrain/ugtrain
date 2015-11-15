/* cfgentry.h:     classes for config read from a file
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
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

// local includes
#include <common.h>


class GrowEntry {
public:
	size_t size_min;
	size_t size_max;
	enum grow_type type;
	u32 add;
	ptr_t code_addr;
	ptr_t code_offs;              // for late PIC support
	ptr_t stack_offs;
	char *lib;                    // for PIC support

	// later determined values
	vector<size_t> v_msize;       // set by malloc/realloc calls
};

class CfgEntry;

class DynMemEntry {
public:
	string name;
	size_t mem_size;
	ptr_t code_addr;
	ptr_t code_offs;              // for late PIC support
	ptr_t stack_offs;
	char *lib;                    // for PIC support
	GrowEntry *grow;

	// later determined values
	vector<ptr_t> v_maddr;        // set by malloc calls
	u32 num_alloc;                // how many obj. created at once
	u32 num_freed;                // how many obj. freed at once
	u32 obj_idx;                  // object index for object check
	u32 pr_idx;                   // object print index

	// adaption
	u32  adp_size;                // adapted object size
	ptr_t adp_addr;               // adapted code address
	ptr_t adp_soffs;              // adapted reverse stack offset
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
	vector<ptr_t> v_offs;
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

struct type {
	bool is_signed;
	bool is_float;
	i32 size;
};

typedef union {
	char		i8;
	unsigned char	u8;
	short		i16;
	unsigned short	u16;
	int		i32;
	unsigned int	u32;
	long long	i64;
	unsigned long long u64;
	float		f32;
	double		f64;
	ptr_t		ptr;
} value_t;

#define MAX_CHK_VALS 4                // for "or" checks

class CheckEntry {
public:
	ptr_t addr;
	CfgEntry *cfg_ref;
	bool is_objcheck;
	bool check_obj_num;
	struct type type;
	check_e check[MAX_CHK_VALS + 1];
	value_t value[MAX_CHK_VALS + 1];
};

class CfgEntry {
public:
	string name;
	ptr_t addr;
	struct type type;
	check_e check;
	dynval_e dynval;
	ptr_t val_addr;               // for DYN_VAL_ADDR only
	value_t value;
	value_t old_val;              // for static memory only

	// get value from other cfg entry (DYN_VAL_ADDR)
	CfgEntry *cfg_ref;
	// more checks
	list<CheckEntry> *checks;
	// dynamic memory
	DynMemEntry *dynmem;
	vector<value_t> v_oldval;     // old value per object
	// memory behind pointer following
	PtrMemEntry *ptrmem;
	PtrMemEntry *ptrtgt;
};

#endif
