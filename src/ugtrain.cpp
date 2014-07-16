/* ugtrain.cpp:    freeze values in process memory (game trainer)
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
 *
 * By the original authors of ugtrain there shall be ABSOLUTELY
 * NO RESPONSIBILITY or LIABILITY for derived work and/or abusive
 * or malicious use. The ugtrain is an education project and
 * sticks to the copyright law by providing configs for games
 * which ALLOW CHEATING only. We can't and we don't accept abusive
 * configs or codes which might turn ugtrain into a cracker tool!
 */

#include <fstream>
#include <list>
#include <vector>
#include <cstring>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>

// local includes
#include "common.h"
// input
#include "options.h"
#include "cfgentry.h"
#include "cfgparser.h"
#include "getch.h"
#include "fifoparser.h"
// processing
#include "system.h"
#include "preload.h"
#include "control.h"
#include "memmgmt.h"
#include "memattach.h"
// output
#include "cfgoutput.h"
#include "valoutput.h"
// special features
#include "discovery.h"
#include "adaption.h"

#define HOME_VAR   "HOME"
#define DYNMEM_IN  "/tmp/memhack_out"
#define DYNMEM_OUT "/tmp/memhack_in"


static inline void memattach_err_once (pid_t pid)
{
	static bool reported = false;

	if (!reported) {
		cerr << "MEMORY ATTACH ERROR PID[" << pid << "]!" << endl;
		reported = true;
	}
}

static inline i32 read_memory (pid_t pid, ptr_t mem_addr, value_t *buf, const char *pfx)
{
	i32 ret;

	ret = memread(pid, mem_addr, buf, sizeof(value_t));
	if (ret)
		cerr << pfx << " READ ERROR PID[" << pid << "] ("
		     << hex << mem_addr << dec << ")!" << endl;
	return ret;
}

static inline i32 write_memory (pid_t pid, ptr_t mem_addr, value_t *buf, const char *pfx)
{
	i32 ret;

	ret = memwrite(pid, mem_addr, buf, sizeof(value_t));
	if (ret)
		cerr << pfx << " WRITE ERROR PID[" << pid << "] ("
		     << hex << mem_addr << dec << ")!" << endl;
	return ret;
}

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

	if (type->is_float) {
		switch (type->size) {
		case 64:
			return check_mem_val(chk_buf->f64, chk_en->value[i].f64, chk_en->check[i]);
		case 32:
			return check_mem_val(chk_buf->f32, chk_en->value[i].f32, chk_en->check[i]);
		default:
			break;
		}
	} else if (type->is_signed) {
		switch (type->size) {
		case 64:
			return check_mem_val(chk_buf->i64, chk_en->value[i].i64, chk_en->check[i]);
		case 32:
			return check_mem_val(chk_buf->i32, chk_en->value[i].i32, chk_en->check[i]);
		case 16:
			return check_mem_val(chk_buf->i16, chk_en->value[i].i16, chk_en->check[i]);
		case 8:
			return check_mem_val(chk_buf->i8, chk_en->value[i].i8, chk_en->check[i]);
		default:
			break;
		}
	} else {
		switch (type->size) {
		case 64:
			return check_mem_val(chk_buf->u64, chk_en->value[i].u64, chk_en->check[i]);
		case 32:
			return check_mem_val(chk_buf->u32, chk_en->value[i].u32, chk_en->check[i]);
		case 16:
			return check_mem_val(chk_buf->u16, chk_en->value[i].u16, chk_en->check[i]);
		case 8:
			return check_mem_val(chk_buf->u8, chk_en->value[i].u8, chk_en->check[i]);
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
	DynMemEntry *dynmem;

	if (cfg_ref->dynmem) {
		dynmem = cfg_ref->dynmem;
		if (cfg_ref->v_oldval.size() <= 0)
			goto err;
		*buf = cfg_ref->v_oldval[dynmem->obj_idx];
	} else if (cfg_ref->ptrmem && cfg_ref->ptrmem->dynmem) {
		dynmem = cfg_ref->ptrmem->dynmem;
		if (cfg_ref->v_oldval.size() <= 0)
			goto err;
		*buf = cfg_ref->v_oldval[dynmem->obj_idx];
	} else {
		*buf = cfg_ref->old_val;
	}
	return 0;
err:
	return -1;
}

static i32 process_checks (pid_t pid, DynMemEntry *dynmem,
			   list<CheckEntry> *chk_lp,
			   ptr_t mem_offs)
{
	list<CheckEntry>::iterator it;
	CheckEntry *chk_en;
	value_t __chk_buf, *chk_buf = &__chk_buf;
	ptr_t mem_addr;
	i32 ret = 0;

	list_for_each (chk_lp, it) {
		chk_en = &(*it);
		if (chk_en->cfg_ref) {
			ret = handle_cfg_ref(chk_en->cfg_ref, chk_buf);
			if (ret)
				continue;
		} else {
			mem_addr = mem_offs + chk_en->addr;

			ret = read_memory(pid, mem_addr, chk_buf, "MEMORY");
			if (ret)
				goto out;
		}
		ret = or_check_memory(chk_en, chk_buf);
		if (ret) {
			// Parser must ensure (dynmem != NULL)
			if (chk_en->is_objcheck)
				dynmem->v_maddr[dynmem->obj_idx] = 0;
			goto out;
		}
	}
out:
	return ret;
}

template <typename T>
static void change_mem_val (pid_t pid, CfgEntry *cfg_en, T read_val, T value,
			    value_t *buf, ptr_t mem_offs)
{
	list<CheckEntry> *chk_lp = cfg_en->checks;
	ptr_t mem_addr;
	i32 ret;

	if (cfg_en->dynval == DYN_VAL_WATCH)
		goto out;

	if (chk_lp) {
		ret = process_checks(pid, cfg_en->dynmem, chk_lp, mem_offs);
		if (ret)
			goto out;
	}

	ret = check_mem_val(read_val, value, cfg_en->check);
	if (ret)
		goto out;

	memcpy(buf, &value, sizeof(T));
	mem_addr = mem_offs + cfg_en->addr;

	ret = write_memory(pid, mem_addr, buf, "MEMORY");
	if (ret)
		goto out;
out:
	return;
}

template <typename T>
static void handle_dynval (pid_t pid, CfgEntry *cfg_en, T read_val,
			   T *value, ptr_t mem_offs)
{
	value_t __buf, *buf = &__buf;
	ptr_t mem_addr = 0;

	__buf.i64 = 0;

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
	default:
		break;
	}
out:
	return;
}

static void change_memory (pid_t pid, CfgEntry *cfg_en, value_t *buf,
			   ptr_t mem_offs, value_t *old_val)
{
	struct type *type = &cfg_en->type;

	if (type->is_float) {
		switch (type->size) {
		case 64:
			old_val->f64 = buf->f64;
			handle_dynval(pid, cfg_en, old_val->f64, &cfg_en->value.f64, mem_offs);
			change_mem_val(pid, cfg_en, old_val->f64, cfg_en->value.f64, buf, mem_offs);
			break;
		case 32:
			old_val->f32 = buf->f32;
			handle_dynval(pid, cfg_en, old_val->f32, &cfg_en->value.f32, mem_offs);
			change_mem_val(pid, cfg_en, old_val->f32, cfg_en->value.f32, buf, mem_offs);
			break;
		}
	} else if (type->is_signed) {
		switch (type->size) {
		case 64:
			old_val->i64 = buf->i64;
			handle_dynval(pid, cfg_en, old_val->i64, &cfg_en->value.i64, mem_offs);
			change_mem_val(pid, cfg_en, old_val->i64, cfg_en->value.i64, buf, mem_offs);
			break;
		case 32:
			old_val->i32 = buf->i32;
			handle_dynval(pid, cfg_en, old_val->i32, &cfg_en->value.i32, mem_offs);
			change_mem_val(pid, cfg_en, old_val->i32, cfg_en->value.i32, buf, mem_offs);
			break;
		case 16:
			old_val->i16 = buf->i16;
			handle_dynval(pid, cfg_en, old_val->i16, &cfg_en->value.i16, mem_offs);
			change_mem_val(pid, cfg_en, old_val->i16, cfg_en->value.i16, buf, mem_offs);
			break;
		default:
			old_val->i8 = buf->i8;
			handle_dynval(pid, cfg_en, old_val->i8, &cfg_en->value.i8, mem_offs);
			change_mem_val(pid, cfg_en, old_val->i8, cfg_en->value.i8, buf, mem_offs);
			break;
		}
	} else {
		switch (type->size) {
		case 64:
			old_val->u64 = buf->u64;
			handle_dynval(pid, cfg_en, old_val->u64, &cfg_en->value.u64, mem_offs);
			change_mem_val(pid, cfg_en, old_val->u64, cfg_en->value.u64, buf, mem_offs);
			break;
		case 32:
			old_val->u32 = buf->u32;
			handle_dynval(pid, cfg_en, old_val->u32, &cfg_en->value.u32, mem_offs);
			change_mem_val(pid, cfg_en, old_val->u32, cfg_en->value.u32, buf, mem_offs);
			break;
		case 16:
			old_val->u16 = buf->u16;
			handle_dynval(pid, cfg_en, old_val->u16, &cfg_en->value.u16, mem_offs);
			change_mem_val(pid, cfg_en, old_val->u16, cfg_en->value.u16, buf, mem_offs);
			break;
		default:
			old_val->u8 = buf->u8;
			handle_dynval(pid, cfg_en, old_val->u8, &cfg_en->value.u8, mem_offs);
			change_mem_val(pid, cfg_en, old_val->u8, cfg_en->value.u8, buf, mem_offs);
			break;
		}
	}
}

// TIME CRITICAL! Process all activated config entries from pointer
static void process_ptrmem (pid_t pid, CfgEntry *cfg_en, value_t *buf, u32 mem_idx)
{
	ptr_t mem_addr;
	list<CfgEntry*> *cfg_act = &cfg_en->ptrtgt->cfg_act;
	list<CfgEntry*>::iterator it;

	if (buf->ptr == 0 || cfg_en->ptrtgt->v_state[mem_idx] == PTR_DONE)
		return;

	if (buf->ptr == cfg_en->v_oldval[mem_idx].ptr) {
		if (cfg_en->dynval == DYN_VAL_PTR_ONCE)
			cfg_en->ptrtgt->v_state[mem_idx] = PTR_DONE;
		else
			cfg_en->ptrtgt->v_state[mem_idx] = PTR_SETTLED;
		cfg_en->ptrtgt->v_offs[mem_idx] = buf->ptr;
		list_for_each (cfg_act, it) {
			cfg_en = *it;
			mem_addr = cfg_en->ptrmem->v_offs[mem_idx] + cfg_en->addr;
			if (read_memory(pid, mem_addr, buf, "PTR MEMORY"))
				continue;
			change_memory(pid, cfg_en, buf, cfg_en->ptrmem->v_offs[mem_idx],
				      &cfg_en->v_oldval[mem_idx]);
		}
	} else {
		cfg_en->v_oldval[mem_idx].ptr = buf->ptr;
	}
}

// TIME CRITICAL! Process all activated config entries
static void process_act_cfg (pid_t pid, list<CfgEntry*> *cfg_act)
{
	list<CfgEntry*>::iterator it;
	CfgEntry *cfg_en;
	value_t __buf, *buf = &__buf;
	ptr_t mem_addr, mem_offs;
	u32 mem_idx;

	__buf.i64 = 0;

	list_for_each (cfg_act, it) {
		cfg_en = *it;
		if (cfg_en->dynmem) {
			for (mem_idx = 0;
			     mem_idx < cfg_en->dynmem->v_maddr.size();
			     mem_idx++) {
				mem_offs = cfg_en->dynmem->v_maddr[mem_idx];
				if (!mem_offs)
					continue;
				cfg_en->dynmem->obj_idx = mem_idx;

				mem_addr =  mem_offs + cfg_en->addr;
				if (read_memory(pid, mem_addr, buf, "MEMORY"))
					continue;
				if (cfg_en->ptrtgt)
					process_ptrmem(pid, cfg_en, buf, mem_idx);
				else
					change_memory(pid, cfg_en, buf, mem_offs,
						&cfg_en->v_oldval[mem_idx]);
			}
		} else {
			mem_offs = 0;

			mem_addr = cfg_en->addr;
			if (read_memory(pid, mem_addr, buf, "MEMORY"))
				continue;
			change_memory(pid, cfg_en, buf, mem_offs, &cfg_en->old_val);
		}
	}
}

static inline void handle_statmem_pie (ptr_t code_offs, list<CfgEntry> *cfg)
{
	list<CfgEntry>::iterator it;
	list<CheckEntry> *chk_lp;
	list<CheckEntry>::iterator chk_it;

	list_for_each (cfg, it) {
		if (it->dynmem || it->ptrmem)
			continue;
		it->addr += code_offs;
		if (!it->checks)
			continue;
		chk_lp = it->checks;
		list_for_each (chk_lp, chk_it)
			chk_it->addr += code_offs;
	}
}

/*
 * The function run_pgrp_bg() is so hacky OS security
 * bypassing so that it is not possible to wait for the
 * child process (the game) in a regular way. We have
 * to do that here in an equal hacky way as the process
 * belongs to init.
 */
static void wait_orphan (pid_t pid, char *proc_name)
{
	while (true) {
		if (!pid_is_running(pid, pid, proc_name, false))
			return;
		sleep_sec(1);
	}
}

static void cmd_str_to_cmd_vec (string *cmd_str, vector<string> *cmd_vec)
{
	u32 start, pos;
	char ch, prev_ch = ' ';

	for (start = 0, pos = 0; pos < cmd_str->size(); pos++) {
		ch = cmd_str->at(pos);
		if (ch == ' ' || ch == '\t') {
			if (prev_ch == ' ')
				continue;
			cmd_vec->push_back(cmd_str->substr(start, pos - start));
			prev_ch = ' ';
		} else {
			if (prev_ch == ' ')
				start = pos;
			prev_ch = ch;
		}
	}
	if (prev_ch != ' ')
		cmd_vec->push_back(cmd_str->substr(start, pos - start));
}

static pid_t run_game (struct app_options *opt, char *preload_lib)
{
	pid_t pid = -1;
	string cmd_str;
	vector<string> cmd_vec;

	if (opt->pre_cmd) {
		if (opt->use_glc) {
			cmd_str += GLC_PRELOADER;
			cmd_str += " ";
		}
		cmd_str += opt->pre_cmd;
		cmd_str += " ";
	}
	cmd_str += opt->game_path;
	if (opt->game_params) {
		cmd_str += " ";
		cmd_str += opt->game_params;
	}

	cmd_str_to_cmd_vec(&cmd_str, &cmd_vec);
	if (!cmd_vec.size()) {
		goto err;
	} else {
		const char *cmd = cmd_vec[0].c_str();
		char *cmdv[cmd_vec.size() + 1];
		u32 i;

		for (i = 0; i < cmd_vec.size(); i++)
			cmdv[i] = to_c_str(&cmd_vec[i]);
		cmdv[i] = NULL;

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

			cout << "$ " << pcmdv[0] << " " << pcmdv[1]
			     << " `pidof -s " << opt->proc_name << "` & --> "
			     << "$ " << cmd_str << " &" << endl;

			pid = run_pgrp_bg(pcmd, pcmdv, cmd, cmdv,
					  pid_str, opt->proc_name, 3,
					  false, preload_lib);
			if (pid > 0)
				opt->scanmem_pid = pid;
		} else {
			cout << "$ " << cmd_str << " &" << endl;

			pid = run_cmd_bg(cmd, cmdv, false, preload_lib);
		}
	}
	if (pid < 0)
		goto err;

	sleep_sec(1);
	if (!pid_is_running(pid, pid, NULL, true))
		pid = 0;    // The game process has been forked by a loader?!

	return pid;
err:
	cerr << "Error while running the game!" << endl;
	return -1;
}

#ifdef __linux__
static pid_t run_preloader (struct app_options *opt)
{
	pid_t pid;

	configure_libmem(opt);

	pid = run_game(opt, opt->preload_lib);
	return pid;
}
#endif

/* returns: 0: success, 1: error, 2: pure static memory detected */
static i32 prepare_dynmem (struct app_options *opt, list<CfgEntry> *cfg,
			   i32 *ifd, i32 *ofd, pid_t *pid)
{
	char obuf[PIPE_BUF] = { 0 };
	u32 num_cfg = 0, num_cfg_len = 0, pos = 0;
	ptr_t old_code_addr = 0;
	list<CfgEntry>::iterator it;
#ifdef __linux__
	size_t written;
#endif

	// Check for discovery first
	if (opt->disc_str) {
		pos += snprintf(obuf + pos, sizeof(obuf) - pos, "%s",
				opt->disc_str);
		if (pos + 2 > sizeof(obuf)) {
			fprintf(stderr, "Buffer overflow\n");
			return 1;
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
		if (it->dynmem && it->dynmem->code_addr != old_code_addr) {
			num_cfg++;
			pos += snprintf(obuf + pos, sizeof(obuf) - pos,
				";%lu;" SCN_PTR, (ulong) it->dynmem->mem_size,
				it->dynmem->code_addr);
				pos += snprintf(obuf + pos, sizeof(obuf) - pos,
					";" SCN_PTR, it->dynmem->stack_offs);
			old_code_addr = it->dynmem->code_addr;
		}
	}
	// Put the number of cfgs to the end
	num_cfg_len = snprintf(obuf + pos, sizeof(obuf) - pos, "%d", num_cfg);
	pos += num_cfg_len;
	if (pos + num_cfg_len + 2 > sizeof(obuf)) {
		fprintf(stderr, "Buffer overflow\n");
		return 1;
	}
	memmove(obuf + num_cfg_len, obuf, pos);  // shift str in buffer right
	memmove(obuf, obuf + pos, num_cfg_len);  // move the number of cfgs to the front
	obuf[pos++] = '\n';  // add cfg end
	obuf[pos++] = '\0';

	if (num_cfg <= 0)
		return 2;

skip_memhack:
#ifdef __linux__
	// Remove FIFOs first for empty FIFOs
	if ((unlink(DYNMEM_IN) && errno != ENOENT) ||
	    (unlink(DYNMEM_OUT) && errno != ENOENT)) {
		perror("unlink FIFO");
		return 1;
	}

	// Set up and open FIFOs
	if (mkfifo(DYNMEM_IN, S_IRUSR | S_IWUSR |
	    S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0 && errno != EEXIST) {
		perror("input mkfifo");
		return 1;
	}
	// security in Ubuntu: mkfifo ignores mode
	if (chmod(DYNMEM_IN, S_IRUSR | S_IWUSR |
	    S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0) {
		perror("input chmod");
		return 1;
	}

	if (mkfifo(DYNMEM_OUT, S_IRUSR | S_IWUSR |
	    S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0 && errno != EEXIST) {
		perror("output mkfifo");
		return 1;
	}
	// security in Ubuntu: mkfifo ignores mode
	if (chmod(DYNMEM_OUT, S_IRUSR | S_IWUSR |
	    S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0) {
		perror("output chmod");
		return 1;
	}

	*ifd = open(DYNMEM_IN, O_RDONLY | O_NONBLOCK);
	if (*ifd < 0) {
		perror("open ifd");
		return 1;
	}

	// Run the preloaded game but not as root
	if (opt->preload_lib && getuid() != 0) {
		cout << "Starting game with " << opt->preload_lib
		     << " preloaded.." << endl;
		*pid = run_preloader(opt);
		if (*pid == 0)
			*pid = -1;
		else if (*pid < 0)
			return 1;
	}

	cout << "Waiting for preloaded library.." << endl;
	*ofd = open(DYNMEM_OUT, O_WRONLY | O_TRUNC);
	if (*ofd < 0) {
		perror("open ofd");
		return 1;
	}

	// Write dynmem cfg to output FIFO
	written = write(*ofd, obuf, pos);
	if (written < pos) {
		perror("FIFO write");
		return 1;
	}
#endif
	return 0;
}

static inline bool tool_is_available (char *name)
{
	bool ret = false;
	char *tmp_path = get_abs_app_path(name);
	cout << "Checking if " << name << " is available: ";
	if (!tmp_path) {
		cout << "no" << endl;
		cerr << "Please consider installing " << name << "!" << endl;
	} else {
		cout << "yes" << endl;
		free(tmp_path);
		ret = true;
	}
	return ret;
}

i32 main (i32 argc, char **argv, char **env)
{
	vector<string> __lines, *lines = &__lines;
	struct app_options __opt, *opt = &__opt;
	list<CfgEntry> __cfg, *cfg = &__cfg;
	list<CfgEntry*> __cfg_act, *cfg_act = &__cfg_act;
	list<CfgEntry*> *cfgp_map[128] = { NULL };
	string input_str;
	pid_t pid, call_pid = -1, worker_pid;
	char def_home[] = "~";
	i32 ret, pmask = PARSE_M | PARSE_C;
	char ch;
	i32 ifd = -1, ofd = -1;
	bool allow_empty_cfg = false;
	ssize_t rbytes;
	bool use_wait = true;
	list<struct region> rlist;

	atexit(restore_getch);

	parse_options(argc, argv, opt);

	opt->home = getenv(HOME_VAR);
	if (!opt->home)
		opt->home = def_home;

	read_config(opt, cfg, cfg_act, cfgp_map, lines);
	cout << "Found config for \"" << opt->proc_name << "\"." << endl;

	if (opt->disc_str) {
		if (opt->disc_str[0] >= '0' && opt->disc_str[0] <= '4') {
			if (opt->run_scanmem &&
			    !tool_is_available((char *) "scanmem"))
				return -1;
			if (opt->disc_str[0] >= '3') {
				opt->have_objdump = tool_is_available(
					(char *) "objdump");
				if (!opt->have_objdump)
					cerr << "Backtrace helpers and symbol "
					    "lookup aren't available." << endl;
			}
			cout << "Clearing config for discovery!" << endl;
			cfg->clear();
			cfg_act->clear();
			allow_empty_cfg = true;
		} else {
			opt->run_scanmem = false;
		}
	} else if (opt->run_scanmem) {
		if (!tool_is_available((char *) "scanmem"))
			return -1;
		cout << "Clearing config for scanmem!" << endl;
		cfg->clear();
		cfg_act->clear();
		allow_empty_cfg = true;
	}

	if (opt->preload_lib) {
		if (!opt->game_path)
			opt->game_path = get_abs_app_path(opt->game_call);
		if (!opt->game_path) {
			cerr << "Absolute game path not found or invalid!"
			     << endl;
			return -1;
		}
	}
	if (!opt->game_binpath)
		opt->game_binpath = opt->game_path;

	cout << "Config:" << endl;
	output_config(cfg);
	cout << endl;
	cout << "Activated:" << endl;
	output_configp(cfg_act);
	cout << endl;

	if (cfg->empty() && !allow_empty_cfg)
		return -1;

	if (prepare_getch() != 0) {
		cerr << "Error while terminal preparation!" << endl;
		return -1;
	}

	ret = process_adaption(opt, cfg, lines);
	if (ret)
		return ret;
discover_next:
	if (prepare_discovery(opt, cfg) != 0)
		return -1;

prepare_dynmem:
	ret = prepare_dynmem(opt, cfg, &ifd, &ofd, &call_pid);
	if (ret == 2) {
		opt->pure_statmem = true;
	} else if (ret != 0) {
		cerr << "Error while dyn. mem. preparation!" << endl;
		return -1;
	}

	pid = proc_to_pid(opt->proc_name);
	if (pid < 0) {
		if (call_pid >= 0)
			goto pid_err;
		/* Run the game but not as root */
		if (opt->pure_statmem && opt->preload_lib) {
#ifdef __linux__
			if (getuid() == 0)
				return -1;
#endif
			cout << "Starting the game.." << endl;
			call_pid = run_game(opt, NULL);
			if (call_pid < 0)
				goto pid_err;
			pid = proc_to_pid(opt->proc_name);
			if (pid < 0)
				goto pid_err;
		} else {
			goto pid_err;
		}
	}
	if (call_pid < 0)
		use_wait = false;
	cout << "PID: " << pid << endl;

	if (opt->disc_str) {
		pmask = PARSE_M | PARSE_S | PARSE_C | PARSE_O;
		if (opt->disc_str[0] == 'p')
			opt->disc_str[0] = opt->disc_str[opt->disc_offs];
		if (opt->disc_str[0] == '0') {
			if (opt->scanmem_pid > 0) {
				wait_proc(opt->scanmem_pid);
				// Have you closed scanmem before the game?
				wait_orphan(pid, opt->proc_name);
			} else {
				wait_proc(call_pid);
			}
			return 0;
		} else if (opt->disc_str[0] >= '1' && opt->disc_str[0] <= '4') {
			struct disc_loop_pp dpp = { ifd, opt };
			prepare_backtrace(opt, ifd, ofd, pid, &rlist);
			worker_pid = fork_proc(run_stage1234_loop, &dpp);
			if (opt->scanmem_pid > 0) {
				wait_proc(opt->scanmem_pid);
				// Have you closed scanmem before the game?
				wait_orphan(pid, opt->proc_name);
			} else {
				wait_proc(call_pid);
			}
			kill_proc(worker_pid);
			if (worker_pid < 0)
				return -1;
		} else if (opt->disc_str[0] == '5') {
			prepare_backtrace(opt, ifd, ofd, pid, &rlist);
			run_stage5_loop(cfg, ifd, pmask, call_pid,
					opt->code_offs);
		}
		//list_regions(&rlist);
		ret = postproc_discovery(opt, cfg, lines);
		switch (ret) {
		case DISC_NEXT:
			goto discover_next;
			break;
		case DISC_OKAY:
			pmask = PARSE_M | PARSE_C;
			goto prepare_dynmem;
			break;
		case DISC_EXIT:
			break;
		default:
			return -1;
		}
	} else if (opt->scanmem_pid > 0) {
		wait_proc(opt->scanmem_pid);
		// Have you closed scanmem before the game?
		wait_orphan(pid, opt->proc_name);
		return 0;
	}

	if (opt->do_adapt || opt->disc_str || opt->run_scanmem)
		return -1;

	set_getch_nb(1);

	if (memattach_test(pid) != 0) {
		cerr << "MEMORY ATTACHING TEST ERROR PID[" << pid << "]!" << endl;
		return -1;
	}

	handle_pie(opt, ifd, ofd, pid, &rlist);
	handle_statmem_pie(opt->code_offs, cfg);

	while (true) {
		sleep_sec_unless_input(1, ifd, STDIN_FILENO);
		ch = do_getch();
		handle_input_char(ch, cfgp_map, pid, cfg, cfg_act);

		// get allocated and freed objects (TIME CRITICAL!)
		if (!opt->pure_statmem) {
			do {
				rbytes = read_dynmem_buf(cfg, NULL, ifd, pmask,
					false, opt->code_offs,
					alloc_dynmem_addr, clear_dynmem_addr);
			} while (rbytes > 0);

			// print allocated and freed object counts
			output_dynmem_changes(cfg);
		}

		// check for active config
		if (cfg_act->empty()) {
			if (!pid_is_running(call_pid, pid, opt->proc_name, use_wait))
				return 0;
			continue;
		}

		if (!opt->pure_statmem) {
			// handle objects freed in the game
			free_dynmem(cfg, false);

			// allocate old values per memory object
			alloc_dynmem(cfg);
		}

		if (memattach(pid) != 0) {
			if (!pid_is_running(call_pid, pid, opt->proc_name, use_wait))
				return 0;
			// coming here often when endling the game
			memattach_err_once(pid);
			continue;
		}

		// TIME CRITICAL! Process all activated config entries
		process_act_cfg(pid, cfg_act);

		if (memdetach(pid) != 0) {
			cerr << "MEMORY DETACH ERROR PID[" << pid << "]!" << endl;
			return -1;
		}

		// free memory used for objects kicked out by object check
		if (!opt->pure_statmem)
			free_dynmem(cfg, true);

		// output old values
		output_mem_values(cfg_act);
	}

	return 0;
pid_err:
	cerr << "PID not found or invalid!" << endl;
	return -1;
}
