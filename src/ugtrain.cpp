/* ugtrain.cpp:    freeze values in process memory (game trainer)
 *
 * Copyright (c) 2012..2019 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * By the original authors of ugtrain there shall be ABSOLUTELY
 * NO RESPONSIBILITY or LIABILITY for derived work and/or abusive
 * or malicious use. The ugtrain is an education project and
 * sticks to the copyright law by providing configs for games
 * which ALLOW CHEATING only. We can't and we don't accept abusive
 * configs or codes which might turn ugtrain into a cracker tool!
 */

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <list>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>

// local includes
#include <common.h>
#include <util.h>
// input
#include <lib/getch.h>
#include <cfgentry.h>
#include <cfgparser.h>
#include <fifoparser.h>
#include <options.h>
// processing
#include <lib/list.h>
#include <lib/memattach.h>
#include <lib/preload.h>
#include <lib/system.h>
#include <control.h>
#include <memmgmt.h>
// output
#include <cfgoutput.h>
#include <valoutput.h>
// special features
#include <adapt.h>
#include <aslr.h>
#include <discovery.h>
#include <testing.h>

#define DYNMEM_IN  "/tmp/memhack_out"
#define DYNMEM_OUT "/tmp/memhack_in"
#define MEMDISC_IN "/tmp/memdisc_out"


/* returns:  0: check passed,  1: not passed */
template <typename T>
static inline i32 check_mem_val (T read_val, T value, check_e check)
{
	i32 ret;

	switch (check) {
	case CHECK_LT:
		ret = !(read_val < value);
		break;
	case CHECK_GT:
		ret = !(read_val > value);
		break;
	case CHECK_EQ:
		ret = !(read_val == value);
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

/* returns:  0: check passed,  1: not passed */
static i32 check_memory (CheckEntry *chk_en, value_t *chk_buf, u32 i)
{
	struct type *type = &chk_en->type;
	value_t *value = &chk_en->value[i];
	check_e check = chk_en->check[i];

	if (type->is_float) {
		switch (type->size) {
		case 64:
			return check_mem_val(chk_buf->f64, value->f64, check);
		case 32:
			return check_mem_val(chk_buf->f32, value->f32, check);
		default:
			break;
		}
	} else if (type->is_signed) {
		switch (type->size) {
		case 64:
			return check_mem_val(chk_buf->i64, value->i64, check);
		case 32:
			return check_mem_val(chk_buf->i32, value->i32, check);
		case 16:
			return check_mem_val(chk_buf->i16, value->i16, check);
		case 8:
			return check_mem_val(chk_buf->i8, value->i8, check);
		default:
			break;
		}
	} else {
		switch (type->size) {
		case 64:
			return check_mem_val(chk_buf->u64, value->u64, check);
		case 32:
			return check_mem_val(chk_buf->u32, value->u32, check);
		case 16:
			return check_mem_val(chk_buf->u16, value->u16, check);
		case 8:
			return check_mem_val(chk_buf->u8, value->u8, check);
		default:
			break;
		}
	}
	return 1;
}

/* returns:  0: one check passed,  1: all not passed */
static inline i32 or_check_memory (CheckEntry *chk_en, value_t *chk_buf)
{
	i32 ret;
	u32 i = 0;

	do {
		ret = check_memory(chk_en, chk_buf, i);
	} while (ret && chk_en->check[++i] != CHECK_END);

	return ret;
}

static inline i32 handle_cfg_ref (CfgEntry *cfg_ref, value_t *buf)
{
	PtrMemEntry* ptrmem = cfg_ref->ptrmem;

	if (cfg_ref->dynmem) {
		DynMemEntry *dynmem = cfg_ref->dynmem;
		vector<value_t> *vvec = &cfg_ref->v_oldval;
		if (cfg_ref->val_set)
			vvec = &cfg_ref->v_value;
		if (vvec->size() <= 0)
			goto err;
		*buf = vvec->at(dynmem->obj_idx);
	} else if (ptrmem && ptrmem->dynmem) {
		DynMemEntry *dynmem = ptrmem->dynmem;
		vector<value_t> *vvec = &cfg_ref->v_oldval;
		if (cfg_ref->val_set)
			vvec = &cfg_ref->v_value;
		if (vvec->size() <= 0)
			goto err;
		// Is the pointer not settled?
		if (ptrmem->v_state[dynmem->obj_idx] == PTR_INIT)
			goto err;
		*buf = vvec->at(dynmem->obj_idx);
	} else {
		// Might be a stack/lib value not available yet
		if (cfg_ref->type.on_stack ||
		    (cfg_ref->type.lib && !cfg_ref->type.lib->start))
			goto err;
		*buf = cfg_ref->old_val;
		if (cfg_ref->val_set)
			*buf = cfg_ref->value;
	}
	return 0;
err:
	return -1;
}

/* Skip error message printing until next value successfully read */
bool g_read_error_no_warning = false;

#define FILL_CACHE_AND_BUF(buf, entry, mem_offs, on_fail)		\
	mem_addr = mem_offs + entry->cache->offs;			\
	if (entry->cache->start == PTR_MAX) {				\
		ret = _read_memory(pid, mem_addr,			\
			entry->cache->data, MEM_CHUNK, "MEMORY");	\
		if (ret) {						\
			g_read_error_no_warning = true;			\
			on_fail;					\
		} else {						\
			g_read_error_no_warning = false;		\
		}							\
		entry->cache->start = mem_addr;				\
	}								\
	memcpy(buf, entry->cache_data, entry->type.size / 8)


static i32 process_checks (pid_t pid, CfgEntry *cfg_en,
			   list<CheckEntry> *chk_lp,
			   ptr_t mem_offs)
{
	DynMemEntry *dynmem = cfg_en->dynmem;
	list<CheckEntry>::iterator it;
	CheckEntry *chk_en;
	value_t _chk_buf, *chk_buf = &_chk_buf;
	ptr_t mem_addr;
	i32 ret = 0;

	list_for_each (chk_lp, it) {
		chk_en = &(*it);
		if (chk_en->type.on_stack ||
		    (chk_en->type.lib && !chk_en->type.lib->start)) {
			ret = 1;
			goto out;
		}
		if (chk_en->cfg_ref) {
			ret = handle_cfg_ref(chk_en->cfg_ref, chk_buf);
			if (ret)
				goto out;
		} else if (chk_en->check_obj_num) {
			if (!dynmem)
				continue;
			chk_buf->u32 = dynmem->obj_idx;
		} else {
			// fill cache and fill chk_buf from cache
			FILL_CACHE_AND_BUF(chk_buf, chk_en, mem_offs, goto out);
		}
		ret = or_check_memory(chk_en, chk_buf);
		if (ret) {
			// Parser must ensure (dynmem != NULL)
			if (chk_en->is_objcheck && likely(dynmem))
				dynmem->v_maddr[dynmem->obj_idx] = 0;
			goto out;
		}
	}
out:
	return ret;
}

template <typename T>
static void change_mem_val (pid_t pid, CfgEntry *cfg_en, T read_val, T value,
			    ptr_t mem_offs)
{
	list<CheckEntry> *chk_lp = cfg_en->checks;
	i32 ret;

	if (cfg_en->dynval == DYN_VAL_WATCH)
		goto out;

	if (chk_lp) {
		ret = process_checks(pid, cfg_en, chk_lp, mem_offs);
		if (ret)
			goto out;
	}

	ret = check_mem_val(read_val, value, cfg_en->check);
	if (ret)
		goto out;

	// Invalid value from reference?
	if (cfg_en->cfg_ref && value == 0)
		goto out;

	// set the new value in the cache
	memcpy(cfg_en->cache_data, &value, sizeof(T));
	cfg_en->cache->is_dirty = true;

	cfg_en->val_set = true;
out:
	return;
}

template <typename T>
static void handle_dynval (pid_t pid, CfgEntry *cfg_en, T read_val,
			   T *value, char *cstr, ptr_t mem_offs)
{
	value_t _buf, *buf = &_buf;
	ptr_t mem_addr = 0;

	_buf.i64 = 0;

	switch (cfg_en->dynval) {
	case DYN_VAL_MAX:
		if (read_val > *value)
			*value = read_val;
		break;
	case DYN_VAL_MIN:
		if (read_val < *value)
			*value = read_val;
		break;
	case DYN_VAL_ADDR:
		if (cfg_en->cfg_ref) {
			if (handle_cfg_ref(cfg_en->cfg_ref, buf) != 0)
				*value = 0;
			else
				*value = *(T *) buf;
		} else {
			mem_addr = mem_offs + cfg_en->val_addr;
			if (read_memory(pid, mem_addr, buf, "DYNVAL MEMORY"))
				goto out;
			*value = *(T *) buf;
		}
		break;
	case DYN_VAL_WATCH:
		if (!cfg_en->type.is_cstrp || !cstr)
			break;
		mem_addr = read_val;
		if (!mem_addr)
			break;
		if (memread(pid, mem_addr, cstr, MAX_CSTR))
			ugerr << "STRING READ ERROR PID[" << pid << "] ("
			      << hex << mem_addr << dec << ")!" << endl;
		break;
	default:
		break;
	}
out:
	return;
}

static void change_memory (pid_t pid, CfgEntry *cfg_en, value_t *buf,
			   ptr_t mem_offs, value_t *old_val, value_t *value,
			   char *cstr)
{
	struct type *type = &cfg_en->type;

	cfg_en->val_set = false;

	if (type->is_float) {
		switch (type->size) {
		case 64:
			old_val->f64 = buf->f64;
			handle_dynval(pid, cfg_en, old_val->f64, &value->f64, cstr, mem_offs);
			change_mem_val(pid, cfg_en, old_val->f64, value->f64, mem_offs);
			break;
		case 32:
			old_val->f32 = buf->f32;
			handle_dynval(pid, cfg_en, old_val->f32, &value->f32, cstr, mem_offs);
			change_mem_val(pid, cfg_en, old_val->f32, value->f32, mem_offs);
			break;
		}
	} else if (type->is_signed) {
		switch (type->size) {
		case 64:
			old_val->i64 = buf->i64;
			handle_dynval(pid, cfg_en, old_val->i64, &value->i64, cstr, mem_offs);
			change_mem_val(pid, cfg_en, old_val->i64, value->i64, mem_offs);
			break;
		case 32:
			old_val->i32 = buf->i32;
			handle_dynval(pid, cfg_en, old_val->i32, &value->i32, cstr, mem_offs);
			change_mem_val(pid, cfg_en, old_val->i32, value->i32, mem_offs);
			break;
		case 16:
			old_val->i16 = buf->i16;
			handle_dynval(pid, cfg_en, old_val->i16, &value->i16, cstr, mem_offs);
			change_mem_val(pid, cfg_en, old_val->i16, value->i16, mem_offs);
			break;
		default:
			old_val->i8 = buf->i8;
			handle_dynval(pid, cfg_en, old_val->i8, &value->i8, cstr, mem_offs);
			change_mem_val(pid, cfg_en, old_val->i8, value->i8, mem_offs);
			break;
		}
	} else {
		switch (type->size) {
		case 64:
			old_val->u64 = buf->u64;
			handle_dynval(pid, cfg_en, old_val->u64, &value->u64, cstr, mem_offs);
			change_mem_val(pid, cfg_en, old_val->u64, value->u64, mem_offs);
			break;
		case 32:
			old_val->u32 = buf->u32;
			handle_dynval(pid, cfg_en, old_val->u32, &value->u32, cstr, mem_offs);
			change_mem_val(pid, cfg_en, old_val->u32, value->u32, mem_offs);
			break;
		case 16:
			old_val->u16 = buf->u16;
			handle_dynval(pid, cfg_en, old_val->u16, &value->u16, cstr, mem_offs);
			change_mem_val(pid, cfg_en, old_val->u16, value->u16, mem_offs);
			break;
		default:
			old_val->u8 = buf->u8;
			handle_dynval(pid, cfg_en, old_val->u8, &value->u8, cstr, mem_offs);
			change_mem_val(pid, cfg_en, old_val->u8, value->u8, mem_offs);
			break;
		}
	}
}

#define WRITE_CACHES(mem_type)					\
do {								\
	cache_list = mem_type->cache_list;			\
	list_for_each (cache_list, cait) {			\
		if (!cait->is_dirty)				\
			continue;				\
		cait->is_dirty = false;				\
		if (_write_memory(pid, cait->start,		\
		    cait->data, MEM_CHUNK, "MEMORY"))		\
			continue;				\
	}							\
} while (0)

#define INVALIDATE_CACHES(mem_type)				\
do {								\
	cache_list = mem_type->cache_list;			\
	list_for_each (cache_list, cait)			\
		cait->start = PTR_MAX;				\
} while (0)

// TIME CRITICAL! Process all activated config entries from pointer
static void process_ptrmem (pid_t pid, CfgEntry *cfg_en, value_t *buf, u32 mem_idx)
{
	ptr_t mem_addr, mem_offs = 0;
	PtrMemEntry *ptrtgt = cfg_en->ptrtgt;
	DynMemEntry *dynmem = cfg_en->ptrtgt->dynmem;
	list<CfgEntry*> *cfg_act = &ptrtgt->cfg_act;
	list<CfgEntry*>::iterator it;
	value_t *value;
	i32 ret;

	if (dynmem) {
		vector<ptr_t> *mvec = &dynmem->v_maddr;
		value = &cfg_en->v_oldval[mem_idx];
		mem_offs = mvec->at(mem_idx);
		if (!mem_offs)
			return;
	} else {
		value = &cfg_en->old_val;
	}
	if (buf->ptr == 0 || ptrtgt->v_state[mem_idx] == PTR_DONE)
		return;

	if (buf->ptr == value->ptr) {
		list<CacheEntry> *cache_list;
		list<CacheEntry>::iterator cait;
		list<CheckEntry> *chk_lp = cfg_en->checks;

		if (chk_lp) {
			ret = process_checks(pid, cfg_en,
					     chk_lp, mem_offs);
			if (ret)
				return;
		}
		if (cfg_en->dynval == DYN_VAL_PTR_ONCE)
			ptrtgt->v_state[mem_idx] = PTR_DONE;
		else
			ptrtgt->v_state[mem_idx] = PTR_SETTLED;
		ptrtgt->v_offs[mem_idx] = buf->ptr;

		// invalidate caches
		INVALIDATE_CACHES(ptrtgt);

		list_for_each (cfg_act, it) {
			PtrMemEntry *ptrmem;
			cfg_en = *it;
			ptrmem = cfg_en->ptrmem;

			// fill cache and fill buf from cache
			FILL_CACHE_AND_BUF(buf, cfg_en, ptrmem->v_offs[mem_idx], continue);

			if (dynmem)
				change_memory(pid, cfg_en, buf,
					ptrmem->v_offs[mem_idx],
					&cfg_en->v_oldval[mem_idx],
					&cfg_en->v_value[mem_idx],
					cfg_en->v_cstr[mem_idx]);
			else
				change_memory(pid, cfg_en, buf,
					ptrmem->v_offs[mem_idx],
					&cfg_en->old_val,
					&cfg_en->value,
					cfg_en->cstr);
		}

		// write back caches to target memory
		WRITE_CACHES(ptrtgt);
	} else {
		value->ptr = buf->ptr;
	}
}

// Process the memory allocations from the malloc queue.
static inline void process_mallocs (list<CfgEntry> *cfg, struct mqueue *mq,
				    i32 pmask)
{
	ssize_t pbytes, ilen = 0;
	struct parse_cb pcb = { NULL };

	pcb.mf = alloc_dynmem_addr;

	pbytes = parse_dynmem_buf(cfg, NULL, mq->data, &ilen, mq->end, pmask,
		false, 0, &pcb);
	if (pbytes <= 0 && mq->end > 0)
		ugerr << "Malloc queue parsing error!" << endl;
	// reinit malloc queue
	mq->end = 0;
	memset(mq->data, 0, mq->size);
	// mallocs read -> delay execution to give the game
	// a chance for memory object initialization
	if (pbytes > 0)
		sleep_msec(250);
}

// TIME CRITICAL! Read the memory allocations and freeings from the input FIFO.
// If the buffer fills, the game process hangs.
static inline void read_dynmem_fifo (list<CfgEntry> *cfg,
				     struct dynmem_params *dp,
				     i32 ifd, i32 pmask)
{
	ssize_t rbytes;
	struct parse_cb pcb = { NULL };

	pcb.lf = get_lib_load_addr;
	pcb.mf = queue_dynmem_addr;
	pcb.ff = clear_dynmem_addr;
	pcb.sf = verify_stack_end;

	do {
		rbytes = read_dynmem_buf(cfg, dp, ifd, pmask, false,
			0, &pcb);
	} while (rbytes > 0);
}

// TIME CRITICAL! Process activated dynmem config entries.
// Note: We are attached to the game process and it is frozen.
static inline void
process_dynmem_cfg_act (pid_t pid, list<CfgEntry*> *cfg_act,
			DynMemEntry *dynmem)
{
	value_t _buf, *buf = &_buf;
	list<CfgEntry*>::iterator it;
	list<CacheEntry> *cache_list;
	list<CacheEntry>::iterator cait;
	CfgEntry *cfg_en;
	vector<ptr_t> *mvec = &dynmem->v_maddr;
	ptr_t mem_addr, mem_offs;
	u32 mem_idx;

	_buf.i64 = 0;

	// object by object
	for (mem_idx = 0;
	     mem_idx < mvec->size();
	     mem_idx++) {
		// invalidate caches
		INVALIDATE_CACHES(dynmem);

		// process current object
		list_for_each (cfg_act, it) {
			cfg_en = *it;
			struct type *type = &cfg_en->type;
			size_t mem_size;
			i32 ret;

			mem_offs = mvec->at(mem_idx);
			if (!mem_offs)
				continue;
			// check for out of bounds access
			if (dynmem->grow) {
				GrowEntry *grow = dynmem->grow;
				mem_size = grow->v_msize[mem_idx];
			} else {
				mem_size = dynmem->mem_size;
			}
			if (cfg_en->addr + type->size / 8 > mem_size)
				continue;
			dynmem->obj_idx = mem_idx;

			// fill cache and fill buf from cache
			FILL_CACHE_AND_BUF(buf, cfg_en, mem_offs, continue);

			if (cfg_en->ptrtgt)
				process_ptrmem(pid, cfg_en, buf, mem_idx);
			else
				change_memory(pid, cfg_en, buf, mem_offs,
					&cfg_en->v_oldval[mem_idx],
					&cfg_en->v_value[mem_idx],
					cfg_en->v_cstr[mem_idx]);
		}

		// write back caches to target memory
		WRITE_CACHES(dynmem);
	}
}

// TIME CRITICAL! Process all activated config entries.
// Note: We are attached to the game process and it is frozen.
static void process_act_cfg (Options *opt, pid_t pid, list<CfgEntry*> *cfg_act)
{
	list<CfgEntry*>::iterator it;
	CfgEntry *cfg_en;
	DynMemEntry *old_dynmem = NULL;
	list<LibEntry>::iterator lit;
	list<CacheEntry> *cache_list;
	list<CacheEntry>::iterator cait;
	value_t _buf, *buf = &_buf;
	ptr_t mem_addr, mem_offs = 0;
	i32 ret;

	_buf.i64 = 0;

	// invalidate caches
	INVALIDATE_CACHES(opt);
	if (opt->val_on_stack)
		INVALIDATE_CACHES(opt->stack);
	list_for_each (opt->lib_list, lit)
		INVALIDATE_CACHES(lit);

	list_for_each (cfg_act, it) {
		cfg_en = *it;
		DynMemEntry *dynmem = cfg_en->dynmem;
		if (dynmem) {
			if (dynmem == old_dynmem)
				continue;
			old_dynmem = dynmem;
			process_dynmem_cfg_act(pid, &dynmem->cfg_act, dynmem);
		} else {
			if (cfg_en->type.on_stack ||
			    (cfg_en->type.lib && !cfg_en->type.lib->start))
				continue;

			// fill cache and fill buf from cache
			FILL_CACHE_AND_BUF(buf, cfg_en, mem_offs, continue);

			if (cfg_en->ptrtgt)
				process_ptrmem(pid, cfg_en, buf, 0);
			else
				change_memory(pid, cfg_en, buf, mem_offs,
					&cfg_en->old_val,
					&cfg_en->value,
					cfg_en->cstr);
		}
	}
	// write back caches to target memory
	list_for_each (opt->lib_list, lit)
		WRITE_CACHES(lit);
	if (opt->val_on_stack)
		WRITE_CACHES(opt->stack);
	WRITE_CACHES(opt);
}

static inline void handle_heap_checks (Options *opt,
				       list<CfgEntry> *cfg)
{
	list<CfgEntry>::iterator it;
	list<CheckEntry> *chk_lp;
	list<CheckEntry>::iterator chk_it;

	list_for_each (cfg, it) {
		if (!it->checks)
			continue;
		chk_lp = it->checks;
		list_for_each (chk_lp, chk_it) {
			if (!chk_it->is_heapchk)
				continue;
			if (chk_it->check[0] == CHECK_EQ &&
			    chk_it->check[1] == CHECK_GT) {
				chk_it->value[0].ptr = opt->heap_start;
				chk_it->value[1].ptr = opt->heap_start;
			} else if (chk_it->check[0] == CHECK_LT) {
				chk_it->value[0].ptr = opt->heap_end;
			}
		}
	}
}

static void reset_terminal (void)
{
	const char *cmd = "reset";
	char *cmdv[2] = { (char *) "reset", NULL };

	restore_getch();
	run_cmd(cmd, cmdv);
	prepare_getch_nb();
}

static const string abuse_names[] = { "steam", "" };
/*
 * This can only stop noobs but that is already enough.
 * Professionals know what they do and don't let others
 * catch them too easily.
 */
static void detect_abuse (Options *opt)
{
	i32 i;

	for (i = 0; abuse_names[i] != ""; i++) {
		if (*opt->game_call == abuse_names[i] ||
		    *opt->proc_name == abuse_names[i])
			goto err;
	}
	return;
err:
	ugerr << "Abuse detected! Cannot allow this." << endl;
	exit(-1);
}

static pid_t run_game (Options *opt, char *preload_lib)
{
#define SCANMEM_DELAY_S    3
#define PROC_CHECK_TMO_MS  2800
#define PROC_CHECK_POLL_MS 50
#define PROC_CHECK_CYCLES  (PROC_CHECK_TMO_MS / PROC_CHECK_POLL_MS)

	pid_t pid = -1;
	pid_t old_game_pid = proc_to_pid(opt->proc_name->c_str());
	u32 i, pos;
	enum pstate pstate = PROC_ERR;
	string cmd_str, game_path;
	const char *cmd;
	char prev_ch;

	detect_abuse(opt);

	if (opt->pre_cmd) {
		cmd_str += opt->pre_cmd;
		cmd_str += " ";
	}

	game_path = opt->game_path;
	// insert '\\' in front of spaces
	for (pos = 0, prev_ch = ' '; pos < game_path.size(); pos++) {
		char ch = game_path.at(pos);
		if (ch == ' ' && prev_ch != '\\') {
			game_path.insert(pos, "\\");
			pos++;
		}
		prev_ch = ch;
	}
	cmd_str += game_path;

	if (!opt->game_params->empty()) {
		cmd_str += " ";
		cmd_str += *opt->game_params;
	}

	if (opt->run_scanmem) {
		char pid_str[12] = { '\0' };
		const char *pcmd = (const char *) SCANMEM;
		char *pcmdv[] = {
			(char *) SCANMEM,
			(char *) "-p",
			pid_str,
			NULL
		};

		restore_getch();
		ignore_sigint();

		ugout << "$ " << pcmdv[0] << " " << pcmdv[1]
		      << " `pidof " << *opt->proc_name
		      << " | cut -d \' \' -f 1` & --> "
		      << "$ " << cmd_str << " &" << endl;

		cmd = cmd_str.c_str();
		pid = run_pgrp_bg(pcmd, pcmdv, cmd, NULL, pid_str,
				  opt->proc_name->c_str(), SCANMEM_DELAY_S,
				  false, preload_lib);
		if (pid > 0)
			opt->scanmem_pid = pid;
		else
			reset_sigint();
	} else {
		ugout << "$ " << cmd_str << " &" << endl;

		cmd = cmd_str.c_str();
		pid = run_cmd_bg(cmd, NULL, false, preload_lib);
	}
	if (pid < 0)
		goto err;

	pid = -1;
	for (i = 0; i < PROC_CHECK_CYCLES; i++) {
		sleep_msec(PROC_CHECK_POLL_MS);
		// handling of a loader which may end very late after forking
		pid = proc_to_pid(opt->proc_name->c_str());
		if (pid == old_game_pid)
			pid = -1;
		if (pid < 0)
			continue;
		pstate = check_process(pid, opt->proc_name->c_str());
		if (pstate == PROC_RUNNING)
			break;
	}
	if (pid < 0 || pstate != PROC_RUNNING)
		goto err;

	return pid;
err:
	ugerr << "Error while running the game!" << endl;
	return -1;

#undef PROC_CHECK_CYCLES
#undef PROC_CHECK_POLL_MS
#undef PROC_CHECK_TMO_MS
#undef SCANMEM_DELAY_S
}

#ifdef __linux__
static pid_t run_preloader (Options *opt)
{
	pid_t pid;

	setenv(UGT_GAME_PROC_NAME, opt->proc_name->c_str(), 1);

	pid = run_game(opt, opt->preload_lib);
	return pid;
}

static inline i32 setup_fifo (const char *name)
{
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	if (mkfifo(name, mode) < 0 && errno != EEXIST) {
		perror("mkfifo");
		goto err;
	}
	// security in Ubuntu: mkfifo ignores mode
	if (chmod(name, mode) < 0) {
		perror("fifo chmod");
		goto err;
	}
	return 0;
err:
	return -1;
}
#endif

/* returns: 0: success, 1: error, 2: pure static memory detected */
static i32 prepare_dynmem (Options *opt, list<CfgEntry> *cfg,
			   i32 *ifd, i32 *ofd, i32 *dfd, pid_t *pid)
{
	char obuf[PIPE_BUF] = { 0 };
	u32 num_cfg = 0, num_cfg_len = 0, pos = 0;
	ptr_t old_code_addr = 0, old_grow_addr = 0;
	string *old_lib = NULL;
	list<CfgEntry>::iterator it;
#ifdef __linux__
	size_t written;
#endif

	// Check for discovery first
	if (!opt->disc_str->empty()) {
		pos += snprintf(obuf + pos, sizeof(obuf) - pos, "%s",
				opt->disc_str->c_str());
		if (pos + 2 > sizeof(obuf)) {
			fprintf(stderr, "Buffer overflow\n");
			goto err;
		}
		obuf[pos++] = '\n';  // add cfg end
		obuf[pos++] = '\0';
		goto skip_memhack;
	}

	if (opt->use_gbt) {
		pos += snprintf(obuf + pos, sizeof(obuf) - pos,
			";%s", GBT_CMD);
	}

	// Fill the output buffer with the dynmem cfg
	list_for_each (cfg, it) {
		DynMemEntry *dynmem = it->dynmem;
		GrowEntry *grow;
		if (!dynmem)
			continue;
		grow = dynmem->grow;
		if ((!grow && ((dynmem->code_addr != old_code_addr) ||
			       (dynmem->lib != old_lib))) ||
		    (grow && ((grow->code_addr != old_grow_addr) ||
			       (grow->lib != old_lib)))) {
			num_cfg++;
			pos += snprintf(obuf + pos, sizeof(obuf) - pos,
				";%lu;" SCN_PTR ";" SCN_PTR, (ulong)
				dynmem->mem_size, dynmem->code_addr,
				dynmem->stack_offs);
			if (dynmem->lib)
				pos += snprintf(obuf + pos, sizeof(obuf) - pos,
					";%s", dynmem->lib->c_str());
			else
				pos += snprintf(obuf + pos, sizeof(obuf) - pos,
						";exe");
			if (grow) {
				pos += snprintf(obuf + pos, sizeof(obuf) - pos,
					";grow;%lu;%lu;+%u;" SCN_PTR ";" SCN_PTR,
					(ulong) grow->size_min,	(ulong)
					grow->size_max, grow->add,
					grow->code_addr,
					grow->stack_offs);
				if (grow->lib)
					pos += snprintf(obuf + pos,
						sizeof(obuf) - pos,
						";%s", grow->lib->c_str());
				else
					pos += snprintf(obuf + pos,
						sizeof(obuf) - pos, ";exe");
				old_grow_addr = grow->code_addr;
				old_lib = grow->lib;
			} else {
				old_code_addr = dynmem->code_addr;
				old_lib = dynmem->lib;
			}
		}
	}
	// Put the number of cfgs to the end
	num_cfg_len = snprintf(obuf + pos, sizeof(obuf) - pos, "%d", num_cfg);
	pos += num_cfg_len;
	if (pos + num_cfg_len + 2 > sizeof(obuf)) {
		fprintf(stderr, "Buffer overflow\n");
		goto err;
	}
	memmove(obuf + num_cfg_len, obuf, pos);  // shift str in buffer right
	memmove(obuf, obuf + pos, num_cfg_len);  // move the number of cfgs to the front
	obuf[pos++] = '\n';  // add cfg end
	obuf[pos++] = '\0';

	if (num_cfg <= 0)
		return 2;

skip_memhack:
#ifdef __linux__
	// Remove debug log
	if (unlink(DBG_FILE_NAME) && errno != ENOENT) {
		perror("unlink debug file");
		goto err;
	}

	// Remove FIFOs first for empty FIFOs
	if ((unlink(DYNMEM_IN) && errno != ENOENT) ||
	    (unlink(DYNMEM_OUT) && errno != ENOENT) ||
	    (unlink(MEMDISC_IN) && errno != ENOENT)) {
		perror("unlink FIFO");
		goto err;
	}
	// Set up and open FIFOs
	if (setup_fifo(DYNMEM_IN))
		goto err;
	if (setup_fifo(DYNMEM_OUT))
		goto err;
	if (!opt->disc_str->empty()) {
		if (setup_fifo(MEMDISC_IN))
			goto err;
		*dfd = open(MEMDISC_IN, O_RDONLY | O_NONBLOCK);
		if (*dfd < 0) {
			perror("open dfd");
			goto err;
		}
	}
	*ifd = open(DYNMEM_IN, O_RDONLY | O_NONBLOCK);
	if (*ifd < 0) {
		perror("open ifd");
		goto err;
	}

	// Run the game with a lib preloaded but not as root
	if (opt->preload_lib && getuid() != 0) {
		ugout << "Starting game with " << opt->preload_lib
		      << " preloaded.." << endl;
		*pid = run_preloader(opt);
		if (*pid <= 0)
			goto err;
	} else {
		goto err;
	}
	ugout << "Waiting for preloaded library.." << endl;
	*ofd = open(DYNMEM_OUT, O_WRONLY | O_TRUNC);
	if (*ofd < 0) {
		perror("open ofd");
		goto err;
	}

	// Write dynmem cfg to output FIFO
	written = write(*ofd, obuf, pos);
	if (written < pos) {
		perror("FIFO write");
		goto err;
	}
#endif
	return 0;
err:
	return 1;
}

static inline i32 get_game_paths (Options *opt)
{
	if (opt->preload_lib) {
		if (!opt->game_path)
			opt->game_path = get_abs_app_path(opt->game_call->c_str());
		if (!opt->game_path) {
			ugerr << "Absolute game path not found or invalid!"
			      << endl;
			return -1;
		}
		if (opt->game_binpath->empty())
			*opt->game_binpath = string(opt->game_path);
	}

	return 0;
}

#ifdef __linux__
static void init_user_cfg_dir (char *home)
{
#define EXAMPLES_DIR "/usr/share/doc/ugtrain/examples"
	i32 ret;
	const char *user_cfg_dir;
	string dir_str, cmd_str;

	dir_str = home;
	dir_str += PSEP PHIDE "ugtrain";
	user_cfg_dir = dir_str.c_str();
	if (dir_exists(user_cfg_dir))
		return;

	ugout << "First run of " << PROG_NAME << " detected:" << endl;
	ugout << "+ Creating \""<< user_cfg_dir << "/\"." << endl;
	ret = create_dir(user_cfg_dir);
	if (ret || !dir_exists(EXAMPLES_DIR))
		goto out;
	ugout << "+ Copying examples to \"" << user_cfg_dir << "/\"." << endl;
	cmd_str = "cp -R " EXAMPLES_DIR "/* ";
	cmd_str += dir_str;
	ret = run_cmd(cmd_str.c_str(), NULL);
	if (ret < 0) {
		ugerr << "Command \"" << cmd_str << "\" failed." << endl;
		goto out;
	}
	if (!tool_is_available((char *) "find") ||
	    !tool_is_available((char *) "gunzip"))
		goto out;
	ugout << "+ Extracting examples in \"" << user_cfg_dir << "/\"." << endl;
	cmd_str = "find ";
	cmd_str += user_cfg_dir;
	cmd_str += " -name \"*.gz\" -exec gunzip {} \\; 2>/dev/null";
	run_cmd(cmd_str.c_str(), NULL);
out:
	cout << endl;
	return;
#undef EXAMPLES_DIR
}
#endif

i32 main (i32 argc, char **argv, char **env)
{
	Globals *gbl = get_globals();
	Options *opt = gbl->opt;
	vector<string> *cfg_lines = gbl->cfg_lines;
	list<CfgEntry> *cfg = gbl->cfg;
	list<CfgEntry*> *cfg_act = gbl->cfg_act;
	list<CfgEntry*> **cfgp_map = gbl->cfgp_map;
	struct dynmem_params *dmparams = gbl->dmparams;
	struct list_head *rlist = gbl->rlist;
	struct mqueue *mq = &dmparams->_mqueue;
	pid_t pid = -1;
	i32 ret, pmask = PARSE_S | PARSE_C;
	char ch;
	char *home = NULL;
	i32 ifd = -1, ofd = -1, dfd = -1;
	bool allow_empty_cfg = false;
	enum pstate pstate;

	init_dmparams_early(dmparams);
	init_atexit(restore_getch);
	init_atexit(cleanup_ugtrain_atexit);

	get_home_path(&home);
	if (!home)
		return -1;
#ifdef __linux__
	if (dir_exists(home))
		init_user_cfg_dir(home);
#endif

	parse_options(argc, argv, opt);
	test_optparsing(opt);
	opt->home = home;

	read_config(opt, cfg_lines);
	test_cfgparsing(opt);
	ugout << "Found config for \"" << *opt->proc_name << "\"." << endl;

	if (!opt->disc_str->empty())
		allow_empty_cfg = init_discovery(opt);
	else if (opt->run_scanmem)
		allow_empty_cfg = init_scanmem();

	get_game_paths(opt);

	ugout << "Config:" << endl;
	output_config(opt, cfg);
	ugout << endl;
	ugout << "Activated:" << endl;
	output_config_act(cfg_act);
	ugout << endl;

	if (cfg->empty() && !allow_empty_cfg)
		return -1;
	test_cfgoutput(opt);

	if (prepare_getch() != 0) {
		ugerr << "Error while terminal preparation!" << endl;
		return -1;
	}

	ret = process_adaptation(opt, cfg_lines);
	if (ret)
		return ret;
	cfg_lines->clear();

	ret = get_game_paths(opt);
	if (ret)
		return ret;

	if (prepare_discovery(opt) != 0)
		return -1;

	ret = prepare_dynmem(opt, cfg, &ifd, &ofd, &dfd, &pid);
	if (ret == 2) {
		opt->pure_statmem = true;
	} else if (ret != 0) {
		ugerr << "Error while dyn. mem. preparation!" << endl;
		return -1;
	}

	pid = proc_to_pid(opt->proc_name->c_str());
	if (pid < 0 && (!opt->pure_statmem || !opt->preload_lib)) {
		goto pid_err;
	} else if (opt->pure_statmem && opt->preload_lib) {
		/* Run the game but not as root */
#ifdef __linux__
		if (getuid() == 0)
			return -1;
#endif
		ugout << "Starting the game.." << endl;
		pid = run_game(opt, NULL);
		if (pid < 0)
			goto pid_err;
	}
	ugout << "PID: " << pid << endl;

	if (!opt->disc_str->empty()) {
		process_discovery(opt, ifd, dfd, ofd, pid);
		//list_regions(rlist);
		ret = postproc_discovery(opt);
		return ret;
	} else if (opt->scanmem_pid > 0) {
		wait_orphan(pid, opt->proc_name->c_str());
		wait_proc(opt->scanmem_pid);
		reset_sigint();
		return 0;
	}

	if (opt->do_adapt || !opt->disc_str->empty() || opt->run_scanmem)
		return -1;

	if (set_getch_nb(1) != 0)
		return -1;

	test_memattach(pid, &opt->procmem_fd);

	handle_aslr(opt, cfg, ifd, ofd, pid, rlist);
	handle_statmem_pie(opt, cfg);
	handle_statmem_pic(opt, cfg, false);
	handle_stack_end(opt, cfg, pid, rlist);

	init_dmparams(dmparams, opt, cfg, ofd, pid, rlist);

	// use sleep_sec_unless_input2() also for pure static memory
	if (ifd < 0)
		ifd = STDIN_FILENO;

	while (true) {
		sleep_sec_unless_input2(1, ifd, STDIN_FILENO);
		ch = do_getch();
		if (ch > 0 && ch < CFGP_MAP_SIZE)
			handle_input_char(ch, cfgp_map, pid, opt->procmem_fd, cfg, cfg_act);

		// get allocated and freed objects (TIME CRITICAL!)
		if (!opt->pure_statmem) {
			read_dynmem_fifo(cfg, dmparams, ifd, pmask);
			// print freed object counts
			ret = output_dynmem_changes(cfg);
			if (ret)
				reset_terminal();
		}

		// check for active config
		if (cfg_act->empty()) {
			pstate = check_process(pid, opt->proc_name->c_str());
			if (pstate != PROC_RUNNING && pstate != PROC_ERR)
				goto out;
			continue;
		}

		if (!opt->pure_statmem) {
			// handle objects freed in the game
			free_dynmem(cfg, false);
		}

		if (memattach(pid) != 0) {
			pstate = check_process(pid, opt->proc_name->c_str());
			if (pstate != PROC_RUNNING && pstate != PROC_ERR)
				goto out;
			// coming here often when ending the game
			memattach_err_once(pid);
			continue;
		}

		// read from the FIFO again
		// R/W to invalid heap addresses crashes the game (SIGSEGV).
		// There could have been free() calls before freezing the game.
		if (!opt->pure_statmem)
			read_dynmem_fifo(cfg, dmparams, ifd, pmask);

		// TIME CRITICAL! Process all activated config entries
		process_act_cfg(opt, (opt->procmem_fd >= 0) ? opt->procmem_fd : pid, cfg_act);

		if (memdetach(pid) != 0) {
			pstate = check_process(pid, opt->proc_name->c_str());
			if (pstate != PROC_RUNNING && pstate != PROC_ERR)
				goto out;
			memdetach_err_once(pid);
			continue;
		}

		bool regions_read = false;
		// read the heap region every cycle as it is growing
		if (opt->heap_checks) {
			get_heap_region(opt, pid, rlist);
			regions_read = true;
			handle_heap_checks(opt, cfg);
		}

		// handle late PIC for static memory
		list<LibEntry>::iterator lit;
		list_for_each (opt->lib_list, lit) {
			if (lit->is_loaded)
				continue;
			if (!regions_read)
				get_regions(pid, rlist);
			regions_read = true;
			find_lib_regions(rlist, opt);
			handle_statmem_pic(opt, cfg, true);
		}

		if (!opt->pure_statmem) {
			// print allocated and freed object counts
			ret = output_dynmem_changes(cfg);
			if (ret)
				reset_terminal();
			/* free memory used for objects kicked out by object
			   check or late free() */
			free_dynmem(cfg, true);
		}
		// output old values
		ret = output_mem_values(cfg_act);
		if (ret)
			reset_terminal();

		// process allocated objects from queue
		if (!opt->pure_statmem) {
			process_mallocs(cfg, mq, pmask);
			// print allocated object counts
			ret = output_dynmem_changes(cfg);
			if (ret)
				reset_terminal();

			// allocate old values per memory object
			alloc_dynmem(cfg);
		}

	}
out:
	return 0;
pid_err:
	ugerr << "PID not found or invalid!" << endl;
	return -1;
}
