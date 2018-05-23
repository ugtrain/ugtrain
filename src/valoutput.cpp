/* valoutput.cpp:    print values read from victim process memory
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
#include <vector>

// local includes
#include <valoutput.h>


static void output_mem_val (CfgEntry *cfg_en, ptr_t mem_offs, bool is_dynmem)
{
	DynMemEntry *dynmem;
	struct type *type = &cfg_en->type;

	if (cfg_en->ptrmem) {
		dynmem = cfg_en->ptrmem->dynmem;
		if (dynmem && dynmem->v_maddr.size() > 1)
			cout << " -> " << cfg_en->name << "[" << dynmem->pr_idx << "]"
			     << " at 0x" << hex << cfg_en->addr + mem_offs
			     << ", Data: 0x";
		else
			cout << " -> " << cfg_en->name << " at 0x" << hex
			     << cfg_en->addr + mem_offs
			     << ", Data: 0x";
	} else if (is_dynmem && cfg_en->dynmem->v_maddr.size() > 1) {
		cout << cfg_en->name << "[" << cfg_en->dynmem->pr_idx << "]"
		     << " at 0x" << hex << cfg_en->addr + mem_offs
		     << ", Data: 0x";
	} else {
		cout << cfg_en->name << " at 0x" << hex
		     << cfg_en->addr + mem_offs
		     << ", Data: 0x";
	}

	if (type->is_cstrp && cfg_en->cstr) {
		cout << hex << cfg_en->old_val.ptr << dec
		     << " (\"" << cfg_en->cstr << "\")" << endl;
		return;
	}

	if (type->is_float) {
		if (type->size == 32) {
			cout << hex << cfg_en->old_val.i32 << dec
			     << " (" << cfg_en->old_val.f32 << ")" << endl;
		} else {
			cout << hex << cfg_en->old_val.i64 << dec
			     << " (" << cfg_en->old_val.f64 << ")" << endl;
		}
	} else {
		if (type->size == 64)
			cout << hex << cfg_en->old_val.i64 << dec;
		else
			cout << hex << cfg_en->old_val.i32 << dec;

		if (type->is_signed) {
			switch (type->size) {
			case 64:
				cout << " (" << cfg_en->old_val.i64 << ")" << endl;
				break;
			case 32:
				cout << " (" << cfg_en->old_val.i32 << ")" << endl;
				break;
			case 16:
				cout << " (" << (i32) cfg_en->old_val.i16 << ")" << endl;
				break;
			default:
				cout << " (" << (i32) cfg_en->old_val.i8 << ")" << endl;
				break;
			}
		} else {
			switch (type->size) {
			case 64:
				cout << " (" << cfg_en->old_val.u64 << ")" << endl;
				break;
			case 32:
				cout << " (" << cfg_en->old_val.u32 << ")" << endl;
				break;
			case 16:
				cout << " (" << (u32) cfg_en->old_val.u16 << ")" << endl;
				break;
			default:
				cout << " (" << (u32) cfg_en->old_val.u8 << ")" << endl;
				break;
			}
		}
	}
}

static void output_ptrmem_values (CfgEntry *cfg_en)
{
	DynMemEntry *dynmem = cfg_en->ptrtgt->dynmem;
	ptr_t mem_offs;
	u32 pr_idx = 0;
	list<CfgEntry*> *cfg_act = &cfg_en->ptrtgt->cfg_act;
	list<CfgEntry*>::iterator it;

	if (dynmem)
		pr_idx = dynmem->pr_idx;

	mem_offs = cfg_en->ptrtgt->v_offs[pr_idx];
	if (!mem_offs || cfg_en->ptrtgt->v_state[pr_idx] < PTR_SETTLED)
		return;
	cout << " -> *" << cfg_en->ptrtgt->name << "[" << pr_idx << "]"
	     << " = 0x" << hex << mem_offs << dec << endl;

	list_for_each (cfg_act, it) {
		cfg_en = *it;
		if (dynmem) {
			cfg_en->old_val = cfg_en->v_oldval[pr_idx];
			cfg_en->value = cfg_en->v_value[pr_idx];
			cfg_en->cstr = cfg_en->v_cstr[pr_idx];
		}
		output_mem_val(cfg_en, mem_offs, false);
	}
}

int output_mem_values (list<CfgEntry*> *cfg_act)
{
	list<CfgEntry*>::iterator it;
	CfgEntry *cfg_en;
	DynMemEntry *old_dynmem = NULL;
	vector<ptr_t> *mvec;
	ptr_t mem_offs = 0;
	bool is_dynmem;

	list_for_each (cfg_act, it) {
		cfg_en = *it;
		is_dynmem = false;
		if (cfg_en->dynmem) {
			mvec = &cfg_en->dynmem->v_maddr;
			if (mvec->empty()) {
				continue;
			} else {
				if (cfg_en->dynmem->pr_idx >= mvec->size())
					cfg_en->dynmem->pr_idx = mvec->size() - 1;
				mem_offs = mvec->at(cfg_en->dynmem->pr_idx);
				cfg_en->old_val =
					cfg_en->v_oldval[cfg_en->dynmem->pr_idx];
				cfg_en->value =
					cfg_en->v_value[cfg_en->dynmem->pr_idx];
				cfg_en->cstr =
					cfg_en->v_cstr[cfg_en->dynmem->pr_idx];
				is_dynmem = true;
				if (cfg_en->dynmem != old_dynmem) {
					cout << "*" << cfg_en->dynmem->name << "["
					     << cfg_en->dynmem->pr_idx << "]" << " = 0x"
					     << hex << mem_offs << dec << ", "
					     << mvec->size() << " obj." << endl;
						old_dynmem = cfg_en->dynmem;
				}
			}
		} else {
			mem_offs = 0;
			if (cfg_en->type.on_stack)
				continue;
		}
		output_mem_val(cfg_en, mem_offs, is_dynmem);
		if (cfg_en->ptrtgt)
			output_ptrmem_values(cfg_en);
		if (cout.fail())
			goto err;
	}
	return 0;
err:
	// Output failed, terminal issue?
	cout.clear();
	return -1;
}
