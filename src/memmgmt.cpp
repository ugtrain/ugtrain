/* memmgmt.cpp:    allocation and freeing of objects and values
 *
 * Copyright (c) 2012..14, by:  Sebastian Parschauer
 *    All rights reserved.     <s.parschauer@gmx.de>
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

#include <vector>

// local includes
#include <memmgmt.h>


static void alloc_ptrmem (CfgEntry *cfg_en)
{
	list<CfgEntry*> *cfg = &cfg_en->ptrtgt->cfg;
	list<CfgEntry*>::iterator it;
	value_t def_val;

	def_val.i64 = 0;

	cfg_en->ptrtgt->v_state.push_back(PTR_INIT);
	cfg_en->ptrtgt->v_offs.push_back(0);

	list_for_each (cfg, it) {
		cfg_en = *it;
		cfg_en->v_oldval.push_back(def_val);
	}
}

void alloc_dynmem (list<CfgEntry> *cfg)
{
	list<CfgEntry>::iterator it;
	CfgEntry *cfg_en;
	u32 mem_idx;
	value_t def_val;

	def_val.i64 = 0;

	list_for_each (cfg, it) {
		cfg_en = &(*it);
		if (!cfg_en->dynmem)
			continue;
		for (mem_idx = 0;
		     mem_idx < cfg_en->dynmem->v_maddr.size();
		     mem_idx++) {
			if (mem_idx >= cfg_en->v_oldval.size()) {
				cfg_en->v_oldval.push_back(def_val);
				if (cfg_en->ptrtgt)
					alloc_ptrmem(cfg_en);
			}
		}
	}
}

static void free_ptrmem (CfgEntry *cfg_en, u32 idx)
{
	list<CfgEntry*> *cfg = &cfg_en->ptrtgt->cfg;
	list<CfgEntry*>::iterator it;

	cfg_en->ptrtgt->v_state.erase(cfg_en->ptrtgt->v_state.begin() + idx);
	cfg_en->ptrtgt->v_offs.erase(cfg_en->ptrtgt->v_offs.begin() + idx);

	list_for_each (cfg, it) {
		cfg_en = *it;
		cfg_en->v_oldval.erase(cfg_en->v_oldval.begin() + idx);
	}
}

void free_dynmem (list<CfgEntry> *cfg, bool process_kicked)
{
	list<CfgEntry>::iterator it;
	CfgEntry *cfg_en;
	DynMemEntry *dynmem, *old_dynmem = NULL;
	GrowEntry *grow;
	vector<ptr_t> *mvec;
	vector<size_t> *svec;
	u32 mem_idx, ov_idx, num_kicked;

	// remove old values marked to be removed by free() or by object check
	list_for_each (cfg, it) {
		cfg_en = &(*it);
		dynmem = cfg_en->dynmem;
		if (!dynmem)
			continue;
		mvec = &dynmem->v_maddr;
		for (mem_idx = 0, ov_idx = 0;
		     mem_idx < mvec->size() &&
		     ov_idx < cfg_en->v_oldval.size();
		     mem_idx++, ov_idx++) {
			if (!mvec->at(mem_idx)) {
				cfg_en->v_oldval.erase(cfg_en->v_oldval.begin()
					+ ov_idx);
				if (cfg_en->ptrtgt)
					free_ptrmem(cfg_en, ov_idx);
				ov_idx--;
			}
		}
	}

	// remove objects marked to be removed by free() or by object check
	old_dynmem = NULL;
	list_for_each (cfg, it) {
		cfg_en = &(*it);
		dynmem = cfg_en->dynmem;
		if (!dynmem || dynmem == old_dynmem)
			continue;
		grow = dynmem->grow;
		mvec = &dynmem->v_maddr;
		num_kicked = 0;
		for (mem_idx = 0; mem_idx < mvec->size(); mem_idx++) {
			if (!mvec->at(mem_idx)) {
				mvec->erase(mvec->begin() + mem_idx);
				if (grow) {
					svec = &grow->v_msize;
					svec->erase(svec->begin() + mem_idx);
				}
				num_kicked++;
				mem_idx--;
			}
		}
		if (process_kicked && num_kicked > 0)
			cout << "===> Obj. " << dynmem->name
			     << " kicked out " << num_kicked
			     << " time(s); remaining: " << mvec->size() << endl;
		old_dynmem = dynmem;
	}
}

void output_dynmem_changes (list<CfgEntry> *cfg)
{
	list<CfgEntry>::iterator it;
	CfgEntry *cfg_en;
	DynMemEntry *dynmem, *old_dynmem = NULL;
	vector<ptr_t> *mvec;

	old_dynmem = NULL;
	list_for_each (cfg, it) {
		cfg_en = &(*it);
		dynmem = cfg_en->dynmem;
		if (!dynmem || dynmem == old_dynmem)
			continue;
		mvec = &dynmem->v_maddr;
		if (dynmem->num_alloc > 0) {
			cout << "===> Obj. " << dynmem->name
			     << " created " << dynmem->num_alloc
			     << " time(s)";
			if (dynmem->num_freed == 0)
				cout << "; now: " << mvec->size();
			cout << endl;
		}
		if (dynmem->num_freed > 0)
			cout << "===> Obj. " << dynmem->name
			     << " freed " << dynmem->num_freed
			     << " time(s); remaining: " << mvec->size() -
				dynmem->num_freed << endl;
		dynmem->num_alloc = 0;
		dynmem->num_freed = 0;
		old_dynmem = dynmem;
	}
}

static i32 find_addr_idx (vector<ptr_t> *vec, ptr_t addr)
{
	u32 i;
	i32 ret = -1;

	for (i = 0; i < vec->size(); i++) {
		if (vec->at(i) == addr) {
			ret = i;
			break;
		}
	}
	return ret;
}

// ff() callback for read_dynmem_buf()
void clear_dynmem_addr (FF_PARAMS)
{
	list<CfgEntry>::iterator it;
	vector<ptr_t> *mvec;
	vector<size_t> *svec;
	i32 idx;

	list_for_each (cfg, it) {
		DynMemEntry *dynmem = it->dynmem;
		GrowEntry *grow;
		if (!dynmem)
			continue;
		grow = dynmem->grow;
		mvec = &dynmem->v_maddr;
		idx = find_addr_idx(mvec, mem_addr);
		if (idx < 0)
			continue;
		mvec->at(idx) = 0;
		if (grow) {
			svec = &grow->v_msize;
			svec->at(idx) = 0;
		}
		it->dynmem->num_freed++;
		break;
	}
}

// mf() callback for read_dynmem_buf()
void alloc_dynmem_addr (MF_PARAMS)
{
	list<CfgEntry>::iterator it;
	vector<ptr_t> *mvec;
	vector<size_t> *svec;

	// find class, allocate and set mem_addr
	list_for_each (cfg, it) {
		DynMemEntry *dynmem = it->dynmem;
		GrowEntry *grow;
		if (!dynmem)
			continue;
		grow = dynmem->grow;
		if (grow) {
			if (dynmem->code_addr != code_addr - code_offs &&
			    grow->code_addr != code_addr - code_offs)
				continue;
			svec = &grow->v_msize;
			svec->push_back(mem_size);
		} else if (dynmem->code_addr != code_addr - code_offs) {
			continue;
		}
		mvec = &dynmem->v_maddr;
		mvec->push_back(mem_addr);
		dynmem->num_alloc++;
		break;
	}
}
