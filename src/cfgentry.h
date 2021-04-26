/* cfgentry.h:     classes for config read from a file
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

#pragma once

#include <vector>
#include <list>

// local includes
#include <common.h>

#ifdef HAVE_PROCMEM
#define MEM_CHUNK 2048                // r/w chunk size for caching target memory
#else
#define MEM_CHUNK sizeof(value_t)
#endif

#define PTR_MAX ULONG_MAX             // value to show that a cache is invalid

class CacheEntry {
public:
	ptr_t offs;                   // offset within dynmem object
	ptr_t start;                  // target memory address or cache valid?
	char *data;                   // target memory r/w buffer
	bool is_dirty;                // writing back required?
};

class LibEntry {
public:
	string name;
	ptr_t start;      // library load address
	list<CacheEntry> *cache_list;
	// late PIC handling
	bool is_loaded;   // for not changing offsets again
	bool skip_val;    // for skipping 1 cycle value output
};

class DumpEntry {
public:
	ptr_t addr;                   // static memory address
	size_t mem_size;
	string *lib_name;             // for PIC support
	LibEntry *lib;
};

class GrowEntry {
public:
	size_t size_min;
	size_t size_max;
	enum grow_type type;
	u32 add;
	ptr_t code_addr;
	ptr_t code_offs;              // for late PIC support
	ptr_t stack_offs;
	string *lib;                  // for PIC support
};

class CfgEntry;

struct DynMemEssentials {
	size_t mem_size;
	ptr_t code_addr;
	ptr_t code_offs;              // for late PIC support
	ptr_t stack_offs;
	string *lib;                  // for PIC support
};

struct DynMemEntry : DynMemEssentials {
	string name;
	vector<DynMemEssentials> *consts;
	GrowEntry *grow;
	list<CfgEntry*> cfg_act;

	// later determined values
	vector<ptr_t> v_maddr;        // set by malloc calls
	vector<size_t> v_msize;       // set by malloc/realloc calls
	u32 num_alloc;                // how many obj. created at once
	u32 num_freed;                // how many obj. freed at once
	u32 obj_idx;                  // object index for object check
	u32 pr_idx;                   // object print index

	// caching
	list<CacheEntry> *cache_list; // object cache list

	// adaptation
	u32 adp_size;                 // adapted object size
	ptr_t adp_addr;               // adapted code address
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

	list<CfgEntry*> cfg;
	list<CfgEntry*> cfg_act;

	DynMemEntry *dynmem;
	vector<ptr_e> v_state;
	vector<ptr_t> v_offs;

	// caching
	list<CacheEntry> *cache_list; // object cache list
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
	string *lib_name;
	LibEntry *lib;
	bool on_stack;
	bool is_signed;
	bool is_float;
	bool is_cstr;
	bool is_cstrp;
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

#define MAX_CHK_VALS      4           // for "or" checks
#define MAX_CSTR        128           // max chars for C string watching

class CheckEntry {
public:
	ptr_t addr;
	CfgEntry *cfg_ref;
	bool is_objcheck;
	bool is_heapchk;
	bool check_obj_num;
	struct type type;
	check_e check[MAX_CHK_VALS + 1];
	value_t value[MAX_CHK_VALS + 1];
	CacheEntry *cache;            // cache entry to be used
	void *cache_data;             // pointer to cache data
};

class CfgEntry {
public:
	string name;
	ptr_t addr;
	struct type type;
	check_e check;
	dynval_e dynval;
	ptr_t val_addr;               // for DYN_VAL_ADDR only
	value_t value;                // for static memory only
	value_t old_val;              // for static memory only
	char *cstr;                   // for static memory only
	bool val_set;                 // Value has been written this cycle?

	CfgEntry *cfg_ref;            // value from another cfg entry (DYN_VAL_ADDR)
	list<CheckEntry> *checks;     // more checks

	// dynamic memory
	DynMemEntry *dynmem;
	vector<value_t> v_value;      // wish value per object
	vector<value_t> v_oldval;     // old value per object
	vector<char*> v_cstr;         // C string per object
	CacheEntry *cache;            // cache entry to be used
	void *cache_data;             // pointer to cache data

	// pointer following
	PtrMemEntry *ptrmem;
	PtrMemEntry *ptrtgt;
};
