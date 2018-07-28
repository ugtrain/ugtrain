/* cfgoutput.cpp:    output functions for the ugtrain config
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

#include <cstring>

// local includes
#include <cfgoutput.h>


static void output_val (struct type *type, value_t value, const char *suffix)
{
	if (type->is_float) {
		if (type->size == 32)
			cout << value.f32 << suffix;
		else
			cout << value.f64 << suffix;
		goto out;
	}
	if (type->is_signed) {
		switch (type->size) {
		case 64:
			cout << value.i64 << suffix;
			break;
		case 32:
			cout << value.i32 << suffix;
			break;
		case 16:
			cout << (i32) value.i16 << suffix;
			break;
		default:
			cout << (i32) value.i8 << suffix;
			break;
		}
	} else {
		switch (type->size) {
		case 64:
			cout << value.u64 << suffix;
			break;
		case 32:
			cout << value.u32 << suffix;
			break;
		case 16:
			cout << (u32) value.u16 << suffix;
			break;
		default:
			cout << (u32) value.u8 << suffix;
			break;
		}
	}
out:
	return;
}

static void output_config_val (CfgEntry *cfg_en)
{
	switch (cfg_en->dynval) {
	case DYN_VAL_WATCH:
		if (cfg_en->type.is_cstrp)
			cout << "cstrp ";
		cout << "(watch)" << endl;
		break;
	case DYN_VAL_MIN:
		output_val(&cfg_en->type, cfg_en->value, " (min)");
		cout << endl;
		break;
	case DYN_VAL_MAX:
		output_val(&cfg_en->type, cfg_en->value, " (max)");
		cout << endl;
		break;
	case DYN_VAL_ADDR:
		if (cfg_en->cfg_ref)
			cout << cfg_en->cfg_ref->name << endl;
		else
			cout << "0x" << hex << cfg_en->value.i64
				<< " (from addr)" << dec << endl;
		break;
	default:
		output_val(&cfg_en->type, cfg_en->value, "");
		cout << endl;
		break;
	}
}

static void output_config_en (CfgEntry *cfg_en, const char *pfx)
{
	if (cfg_en->ptrtgt) {
		ugout << pfx << "  " << cfg_en->name << " 0x" << hex << cfg_en->addr << dec
			<< " " << 8 * sizeof(ptr_t) << "-bit -> "
			<< cfg_en->ptrtgt->name << endl;
	} else {
		ugout << pfx << "  " << cfg_en->name << " ";
		if (cfg_en->type.on_stack)
			cout << "stack 0x";
		else if (cfg_en->type.lib_name && !cfg_en->type.lib_name->empty())
			cout << "lib " << *cfg_en->type.lib_name << " 0x";
		else
			cout << "0x";
		cout << hex << cfg_en->addr << dec
			<< " " << cfg_en->type.size << "-bit ";
		output_config_val(cfg_en);
	}
}

static void output_ptrmem_act (CfgEntry *cfg_en)
{
	list<CfgEntry*> *cfg_act = &cfg_en->ptrtgt->cfg_act;
	list<CfgEntry*>::iterator it;
	const char *pfx = "  ->";

	list_for_each (cfg_act, it) {
		cfg_en = *it;
		output_config_en(cfg_en, pfx);
	}
}

static char *get_check_str (CheckEntry *chk_en)
{
	char *ret = (char *) ((chk_en->is_objcheck) ? " (objcheck)" : "");

	ret = (char *) ((chk_en->is_heapchk) ? " (heapcheck)" : ret);
	return ret;
}

static char *get_check_op (check_e check)
{
	char *check_op;

	switch (check) {
	case CHECK_LT:
		check_op = (char *) " < ";
		break;
	case CHECK_GT:
		check_op = (char *) " > ";
		break;
	case CHECK_EQ:
		check_op = (char *) " == ";
		break;
	default:
		check_op = NULL;
		break;
	}
	return check_op;
}

static void output_checks (CfgEntry *cfg_en)
{
	list<CheckEntry> *chk_lp = cfg_en->checks;
	list<CheckEntry>::iterator it;
	u32 i;
	char *check_op = get_check_op(cfg_en->check);

	if (check_op) {
		ugout << "    check ";
		if (cfg_en->type.on_stack)
			cout << "stack 0x";
		else if (cfg_en->type.lib_name && !cfg_en->type.lib_name->empty())
			cout << "lib " << *cfg_en->type.lib_name << " 0x";
		else
			cout << "0x";
		cout << hex << cfg_en->addr << dec << check_op;
		output_val(&cfg_en->type, cfg_en->value, "");
		cout << endl;
	}
	if (!chk_lp)
		return;

	list_for_each (chk_lp, it) {
		ugout << "    check ";
		if (it->cfg_ref) {
			cout << it->cfg_ref->name;
			goto skip_addr;
		}
		if (it->type.on_stack)
			cout << "stack 0x";
		else if (it->type.lib_name && !it->type.lib_name->empty())
			cout << "lib " << *it->type.lib_name << " 0x";
		else
			cout << "0x";
		cout  << hex << it->addr << dec;
skip_addr:
		for (i = 0; it->check[i] != CHECK_END; i++) {
			if (i > 0)
				cout << " ||";
			check_op = get_check_op(it->check[i]);
			cout << check_op;
			output_val(&it->type, it->value[i], get_check_str(&(*it)));
		}
		cout << endl;
	}
}

void output_config_act (list<CfgEntry*> *cfg_act)
{
	if (!cfg_act || cfg_act->empty()) {
		ugout << "<none>" << endl;
		return;
	}

	CfgEntry *cfg_en;

	list<CfgEntry*>::iterator it;
	list_for_each (cfg_act, it) {
		cfg_en = *it;
		output_config_en(cfg_en, "");
		if (cfg_en->ptrtgt)
			output_ptrmem_act(cfg_en);
	}
}

#define OUTPUT_CACHE(mem_type)						\
do {									\
	list<CacheEntry> *cache_list = mem_type->cache_list;		\
	CacheEntry *cache;						\
	list<CacheEntry>::iterator it;					\
									\
	if (cache_list->front().offs != PTR_MAX) {			\
		cout << " (caches at:";					\
		list_for_each (cache_list, it) {			\
			cache = &(*it);					\
			if (!cache)					\
				continue;				\
			cout << hex << " 0x" << cache->offs << dec;	\
		}							\
		cout << ")";						\
	}								\
} while (0)

static void output_grow_method (GrowEntry *grow)
{
	enum grow_type type = grow->type;

	switch (type) {
	case GROW_ADD:
		cout << "+" << grow->add;
		break;
	default:
		cout << "unknown";
		break;
	}
}

void output_config (Options *opt, list<CfgEntry> *cfg)
{
	if (!cfg || cfg->empty()) {
		ugout << "<none>" << endl;
		return;
	}

	CfgEntry *cfg_en;
	CfgEntry *old_cfg_en = NULL;

	list<CfgEntry>::iterator it;
	list_for_each (cfg, it) {
		cfg_en = &(*it);
		// headline
		if (cfg_en->dynmem) {
			DynMemEntry *dynmem = cfg_en->dynmem;
			GrowEntry *grow = dynmem->grow;
			if (old_cfg_en && dynmem == old_cfg_en->dynmem)
				goto skip_hl;
			ugout << "dynmem: " << dynmem->name << " "
			      << dynmem->mem_size << " 0x" << hex
			      << dynmem->code_addr << " 0x" << dynmem->stack_offs
			      << dec;
			if (dynmem->lib && !dynmem->lib->empty())
				cout << " " << *dynmem->lib;
			if (grow) {
				cout << " growing " << grow->size_min
				     << " " << grow->size_max << " ";
				output_grow_method(grow);
				cout << " 0x" << hex << grow->code_addr << " 0x"
				     << grow->stack_offs << dec;
				if (grow->lib && !grow->lib->empty())
					cout << " " << *grow->lib;
			}
			OUTPUT_CACHE(dynmem);
			cout << endl;
		} else if (cfg_en->ptrmem) {
			PtrMemEntry *ptrmem = cfg_en->ptrmem;
			if (old_cfg_en && ptrmem == old_cfg_en->ptrmem)
				goto skip_hl;
			ugout << "ptrmem: " << ptrmem->name << " "
				<<  ptrmem->mem_size;
			OUTPUT_CACHE(ptrmem);
			cout << endl;
		} else {
			if (old_cfg_en && !old_cfg_en->dynmem && !old_cfg_en->ptrmem)
				goto skip_hl;
			ugout << "static memory:";
			if (opt->val_on_stack)
				OUTPUT_CACHE(opt->stack);
			list<LibEntry>::iterator lit;
			list_for_each (opt->lib_list, lit)
				OUTPUT_CACHE(lit);
			OUTPUT_CACHE(opt);
			cout << endl;
		}
		old_cfg_en = cfg_en;
skip_hl:
		// value line
		output_config_en(cfg_en, "");
		output_checks(cfg_en);
	}
}
