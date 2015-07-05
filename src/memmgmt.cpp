/* memmgmt.cpp:    allocation and freeing of objects and values
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

#include <vector>

// local includes
#include <cstring>
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
		num_kicked -= dynmem->num_freed;
		if (process_kicked && num_kicked > 0)
			cout << "===> Obj. " << dynmem->name
			     << " kicked out " << num_kicked
			     << " time(s); remaining: " << mvec->size() << endl;
		dynmem->num_freed = 0;
		old_dynmem = dynmem;
	}
}

int output_dynmem_changes (list<CfgEntry> *cfg)
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
		if (cout.fail())
			goto err;
		dynmem->num_alloc = 0;
		old_dynmem = dynmem;
	}
	return 0;
err:
	// Output failed, terminal issue?
	cout.clear();
	return -1;
}

static inline i32 find_addr_idx (vector<ptr_t> *vec, ptr_t addr)
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

static inline i32 remove_malloc_from_queue (struct mqueue *mq, ptr_t free_addr)
{
	i32 ret = -1, len;
	char *pstart, *msg_end;
	ptr_t malloc_addr;

	if (!mq->end)
		goto out;
	pstart = mq->data + 1;
next:
	msg_end = strchr(pstart, '\n');
	if (msg_end == NULL)
		goto out;
	msg_end++;
	if (sscanf(pstart, SCN_PTR, &malloc_addr) != 1)
		goto out;
	// Remove the found malloc to be freed from the queue.
	if (malloc_addr == free_addr) {
		pstart--;
		len = mq->end - (msg_end - mq->data) + 1;
		if (len >= mq->end)
			goto out;
		memmove(pstart, msg_end, len);
		len = msg_end - pstart;
		mq->end -= len;

		ret = 0;
		goto out;
	}
	pstart = msg_end + 1;
	if ((pstart - mq->data) >= mq->end)
		goto out;
	goto next;
out:
	return ret;
}


// ff() callback for read_dynmem_buf()
void clear_dynmem_addr (FF_PARAMS)
{
	struct mqueue *mq = (struct mqueue *) argp;
	list<CfgEntry>::iterator it;
	vector<ptr_t> *mvec;
	vector<size_t> *svec;
	i32 idx, ret;

	ret = remove_malloc_from_queue(mq, mem_addr);
	if (!ret)
		return;

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
			if (dynmem->code_addr != code_addr &&
			    grow->code_addr != code_addr)
				continue;
			svec = &grow->v_msize;
			svec->push_back(mem_size);
		} else if (dynmem->code_addr != code_addr) {
			continue;
		}
		mvec = &dynmem->v_maddr;
		mvec->push_back(mem_addr);
		dynmem->num_alloc++;
		break;
	}
}

// mf() callback for read_dynmem_buf()
//
// write a message including '\n' to the malloc queue
void queue_dynmem_addr (MF_PARAMS)
{
	struct mqueue *mq = (struct mqueue *) pp->argp;
	char *msg_start = pp->ibuf;
	char *msg_end = pp->msg_end;
	char *mq_start = mq->data + mq->end;
	char tmp_char;
	ssize_t n = mq->size - mq->end;
	ssize_t max_print = n - 1;
	int np;

	tmp_char = *(msg_end + 1);
	*(msg_end + 1) = '\0';
	np = snprintf(mq_start, n, "%s", msg_start);
	*(msg_end + 1) = tmp_char;
	if (np < 0 || np > max_print)
		mq_start[1] = '\0';
	else
		mq->end += np;
}
