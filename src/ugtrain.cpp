/* ugtrain.cpp:    freeze values in process memory (game trainer)
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
#include "commont.cpp"
#include "options.h"
#include "cfgentry.h"
#include "cfgparser.h"
#include "system.h"
#include "getch.h"
#include "memattach.h"
#include "fifoparser.h"
#include "discovery.h"

#define HOME_VAR   "HOME"
#define DYNMEM_IN  "/tmp/memhack_out"
#define DYNMEM_OUT "/tmp/memhack_in"


void output_configp (list<CfgEntry*> *cfg)
{
	double tmp_dval;

	if (!cfg || cfg->empty()) {
		cout << "<none>" << endl;
		return;
	}

	CfgEntry *cfg_en;

	list<CfgEntry*>::iterator it;
	for (it = cfg->begin(); it != cfg->end(); it++) {
		cfg_en = *it;
		cout << cfg_en->name << " " << hex << cfg_en->addr << dec
			<< " " << cfg_en->size << " ";
		if (cfg_en->is_float) {
			memcpy(&tmp_dval, &cfg_en->value, sizeof(i64));
			cout << tmp_dval << endl;
		} else {
			cout << cfg_en->value << endl;
		}
	}
}

static void output_checks (CfgEntry *cfg_en)
{
	list<CheckEntry> *chk_lp;
	double tmp_dval;

	if (cfg_en->check > DO_UNCHECKED) {
		cout << "check " << hex << cfg_en->addr << dec;
		if (cfg_en->check == DO_LT)
			cout << " <";
		else if (cfg_en->check == DO_GT)
			cout << " >";
	}
	if (cfg_en->is_float) {
		memcpy(&tmp_dval, &cfg_en->value, sizeof(i64));
		cout << " " << tmp_dval << endl;
	} else {
		cout << " " << cfg_en->value << endl;
	}

	if (!cfg_en->checks)
		return;
	chk_lp = cfg_en->checks;
	list<CheckEntry>::iterator it;
	for (it = chk_lp->begin(); it != chk_lp->end(); it++) {
		if (it->check > DO_UNCHECKED) {
			cout << "check " << hex << it->addr << dec;
			if (it->check == DO_LT)
				cout << " <";
			else if (it->check == DO_GT)
				cout << " >";
		}
		if (it->is_float) {
			memcpy(&tmp_dval, &it->value, sizeof(i64));
			cout << " " << tmp_dval
			     << ((it->is_objcheck) ? " (objcheck)" : "") << endl;
		} else {
			cout << " " << it->value
			     << ((it->is_objcheck) ?" (objcheck)" : "") << endl;
		}
	}
}

static void output_config (list<CfgEntry> *cfg)
{
	double tmp_dval;

	if (!cfg || cfg->empty()) {
		cout << "<none>" << endl;
		return;
	}

	CfgEntry cfg_en;

	list<CfgEntry>::iterator it;
	for (it = cfg->begin(); it != cfg->end(); it++) {
		cfg_en = *it;
		if (cfg_en.dynmem)
			cout << "dynmem: " << cfg_en.dynmem->name << " "
				<< cfg_en.dynmem->mem_size << " "
				<< hex << cfg_en.dynmem->code_addr << " "
				<< cfg_en.dynmem->stack_offs << dec << endl;
		cout << cfg_en.name << " " << hex << cfg_en.addr << dec;
		cout << " " << cfg_en.size << " ";
		if (cfg_en.is_float) {
			memcpy(&tmp_dval, &cfg_en.value, sizeof(i64));
			cout << tmp_dval << endl;
		} else {
			cout << cfg_en.value << endl;
		}
		output_checks(&cfg_en);
	}
}

static void dump_mem_obj (pid_t pid, u32 class_id, u32 obj_id,
			  void *mem_addr, size_t size)
{
	i32 fd;
	string fname;
	char buf[size];
	ssize_t wbytes;

	if (memattach(pid) != 0)
		goto err;
	if (memread(pid, mem_addr, buf, sizeof(buf)) != 0)
		goto err_detach;
	memdetach(pid);

	fname = to_string(class_id);
	fname += "_";
	if (obj_id < 100)
		fname += "0";
	if (obj_id < 10)
		fname += "0";
	fname += to_string(obj_id);
	fname += ".dump";

	fd = open(fname.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0)
		goto err;
	wbytes = write(fd, buf, sizeof(buf));
	if (wbytes < (ssize_t) sizeof(buf))
		cerr << fname << ": Write error!" << endl;
	close(fd);
	return;
err_detach:
	memdetach(pid);
err:
	return;
}

static void inc_dec_mvec_pridx (list<CfgEntry> *cfg, bool do_inc)
{
	DynMemEntry *old_dynmem = NULL;
	list<CfgEntry>::iterator it;

	for (it = cfg->begin(); it != cfg->end(); it++) {
		if (it->dynmem && it->dynmem != old_dynmem) {
			if (do_inc)
				it->dynmem->pridx++;
			else if (it->dynmem->pridx > 0)
				it->dynmem->pridx--;
			old_dynmem = it->dynmem;
		}
	}
}

static void dump_all_mem_obj (pid_t pid, list<CfgEntry> *cfg)
{
	DynMemEntry *old_dynmem = NULL;
	u32 class_id = 0, obj_id = 0, i;
	list<CfgEntry>::iterator it;

	for (it = cfg->begin(); it != cfg->end(); it++) {
		if (it->dynmem && it->dynmem != old_dynmem) {
			obj_id = 0;
			for (i = 0; i < it->dynmem->v_maddr.size(); i++) {
				cout << ">>> Dumping Class " << class_id
				     << " Obj " << obj_id << " at "
				     << it->dynmem->v_maddr[obj_id] << endl;
				dump_mem_obj(pid, class_id, obj_id,
					     it->dynmem->v_maddr[obj_id],
					     it->dynmem->mem_size);
				obj_id++;
			}
			class_id++;
			old_dynmem = it->dynmem;
		}
	}
}

static void toggle_cfg (list<CfgEntry*> *cfg, list<CfgEntry*> *cfg_act)
{
	bool found;
	CfgEntry *cfg_en;

	list<CfgEntry*>::iterator it, it_act;
	for (it = cfg->begin(); it != cfg->end(); it++) {
		found = false;
		for (it_act = cfg_act->begin(); it_act != cfg_act->end(); it_act++) {
			if (*it == *it_act) {
				cfg_act->erase(it_act);
				found = true;
				cfg_en = *it_act;
				cout << cfg_en->name << " OFF" << endl;
				break;
			}
		}
		if (!found) {
			cfg_act->push_back(*it);
			cfg_en = *it;
			cout << cfg_en->name << " ON" << endl;
		}
	}
}

static void output_mem_val (CfgEntry *cfg_en, void *mem_offs, bool is_dynmem)
{
	double tmp_dval;
	float  tmp_fval;
	i32 hexfloat;

	if (is_dynmem && cfg_en->dynmem->v_maddr.size() > 1)
		cout << cfg_en->name << "[" << cfg_en->dynmem->pridx << "]"
		     << " at " << hex << PTR_ADD(void *, cfg_en->addr, mem_offs)
		     << ", Data: 0x";
	else
		cout << cfg_en->name << " at " << hex
		     << PTR_ADD(void *, cfg_en->addr, mem_offs)
		     << ", Data: 0x";

	if (cfg_en->is_float) {
		memcpy(&tmp_dval, &cfg_en->old_val, sizeof(i64));
		if (cfg_en->size == 32) {
			tmp_fval = (float) tmp_dval;
			memcpy(&hexfloat, &tmp_fval, sizeof(i32));
			cout << hex << hexfloat;
		} else {
			cout << hex << (i64) cfg_en->old_val;
		}
		cout << " (" << dec << tmp_dval << ")" << endl;
	} else {
		if (cfg_en->size == 64)
			cout << hex << (i64) cfg_en->old_val;
		else
			cout << hex << (i32) cfg_en->old_val;

		if (cfg_en->is_signed)
			cout << " (" << dec << cfg_en->old_val << ")" << endl;
		else
			cout << " (" << dec << (u64) cfg_en->old_val << ")" << endl;
	}
}

template <typename T>
static inline i32 check_mem_val (T value, u8 *chk_buf, i32 check)
{
	if ((check == DO_LT && *(T *)chk_buf < value) ||
	    (check == DO_GT && *(T *)chk_buf > value))
		return 0;
	return -1;
}

static i32 check_memory (CheckEntry chk_en, u8 *chk_buf)
{
	if (chk_en.is_signed) {
		switch (chk_en.size) {
		case 64:
			return check_mem_val((i64) chk_en.value, chk_buf, chk_en.check);
		case 32:
			return check_mem_val((i32) chk_en.value, chk_buf, chk_en.check);
		case 16:
			return check_mem_val((i16) chk_en.value, chk_buf, chk_en.check);
		default:
			return check_mem_val((i8) chk_en.value, chk_buf, chk_en.check);
		}
	} else {
		switch (chk_en.size) {
		case 64:
			return check_mem_val((u64) chk_en.value, chk_buf, chk_en.check);
		case 32:
			return check_mem_val((u32) chk_en.value, chk_buf, chk_en.check);
		case 16:
			return check_mem_val((u16) chk_en.value, chk_buf, chk_en.check);
		default:
			return check_mem_val((u8) chk_en.value, chk_buf, chk_en.check);
		}
	}
}

template <typename T>
static void change_mem_val (pid_t pid, CfgEntry *cfg_en, T value, u8 *buf, void *mem_offs)
{
	list<CheckEntry> *chk_lp;
	u8 chk_buf[sizeof(i64)];
	void *mem_addr;

	if (cfg_en->dynval == DYN_VAL_WATCH)
		return;

	if (cfg_en->checks) {
		chk_lp = cfg_en->checks;
		list<CheckEntry>::iterator it;
		for (it = chk_lp->begin(); it != chk_lp->end(); it++) {
			mem_addr = PTR_ADD(void *, mem_offs, it->addr);

			if (memread(pid, mem_addr, chk_buf, sizeof(i64)) != 0) {
				cerr << "PTRACE READ MEMORY ERROR PID[" << pid << "]!" << endl;
				exit(-1);
			}
			if (check_memory(*it, chk_buf) != 0) {
				if (it->is_objcheck)
					cfg_en->dynmem->v_maddr[cfg_en->dynmem->objidx] = NULL;
				return;
			}
		}
	}

	if ((cfg_en->check == DO_UNCHECKED) ||
	    (cfg_en->check == DO_LT && *(T *)buf < value) ||
	    (cfg_en->check == DO_GT && *(T *)buf > value)) {
		memcpy(buf, &value, sizeof(T));
		mem_addr = PTR_ADD(void *, mem_offs, cfg_en->addr);

		if (memwrite(pid, mem_addr, buf, sizeof(i64)) != 0) {
			cerr << "PTRACE WRITE MEMORY ERROR PID[" << pid << "]!" << endl;
			exit(-1);
		}
	}
}

template <typename T>
static void handle_dynval (pid_t pid, CfgEntry *cfg_en, T read_val,
			   T *value, void *mem_offs)
{
	u8 buf[sizeof(i64)] = { 0 };
	u8 *bufp = buf;  // cheat compiler
	void *mem_addr = NULL;

	if (cfg_en->dynval == DYN_VAL_MAX &&
	    read_val > *value) {
		*value = read_val;
	} else if (cfg_en->dynval == DYN_VAL_MIN &&
	    read_val < *value) {
		*value = read_val;
	} else if (cfg_en->dynval == DYN_VAL_ADDR) {
		mem_addr = PTR_ADD(void *, mem_offs, cfg_en->val_addr);
		if (memread(pid, mem_addr, buf, sizeof(i64)) != 0) {
			cerr << "PTRACE READ MEMORY ERROR PID["
			     << pid << "]!" << endl;
			exit(-1);
		}
		*value = *(T *)bufp;
	}
}

static void change_memory (pid_t pid, CfgEntry *cfg_en, u8 *buf,
			   void *mem_offs, i64 *old_val)
{
	double tmp_dval, old_dval;
	float  tmp_fval, old_fval;

	if (cfg_en->is_float) {
		memcpy(&tmp_dval, &cfg_en->value, sizeof(i64));
		switch (cfg_en->size) {
		case 64:
			old_dval = *(double *)buf;
			handle_dynval(pid, cfg_en, old_dval, &tmp_dval, mem_offs);
			memcpy(old_val, &old_dval, sizeof(i64));
			change_mem_val(pid, cfg_en, tmp_dval, buf, mem_offs);
			break;
		case 32:
			old_fval = *(float *)buf;
			tmp_fval = (float) tmp_dval;
			handle_dynval(pid, cfg_en, old_fval, &tmp_fval, mem_offs);
			old_dval = (double) old_fval;
			tmp_dval = (double) tmp_fval;
			memcpy(old_val, &old_dval, sizeof(i64));
			change_mem_val(pid, cfg_en, tmp_fval, buf, mem_offs);
			break;
		}
		memcpy(&cfg_en->value, &tmp_dval, sizeof(i64));
	} else if (cfg_en->is_signed) {
		switch (cfg_en->size) {
		case 64:
			*old_val = *(i64 *)buf;
			handle_dynval(pid, cfg_en, *(i64 *)buf, (i64 *) &cfg_en->value, mem_offs);
			change_mem_val(pid, cfg_en, (i64) cfg_en->value, buf, mem_offs);
			break;
		case 32:
			*old_val = *(i32 *)buf;
			handle_dynval(pid, cfg_en, *(i32 *)buf, (i32 *) &cfg_en->value, mem_offs);
			change_mem_val(pid, cfg_en, (i32) cfg_en->value, buf, mem_offs);
			break;
		case 16:
			*old_val = *(i16 *)buf;
			handle_dynval(pid, cfg_en, *(i16 *)buf, (i16 *) &cfg_en->value, mem_offs);
			change_mem_val(pid, cfg_en, (i16) cfg_en->value, buf, mem_offs);
			break;
		default:
			*old_val = *(i8 *)buf;
			handle_dynval(pid, cfg_en, *(i8 *)buf, (i8 *) &cfg_en->value, mem_offs);
			change_mem_val(pid, cfg_en, (i8) cfg_en->value, buf, mem_offs);
			break;
		}
	} else {
		switch (cfg_en->size) {
		case 64:
			*old_val = *(u64 *)buf;
			handle_dynval(pid, cfg_en, *(u64 *)buf, (u64 *) &cfg_en->value, mem_offs);
			change_mem_val(pid, cfg_en, (u64) cfg_en->value, buf, mem_offs);
			break;
		case 32:
			*old_val = *(u32 *)buf;
			handle_dynval(pid, cfg_en, *(u32 *)buf, (u32 *) &cfg_en->value, mem_offs);
			change_mem_val(pid, cfg_en, (u32) cfg_en->value, buf, mem_offs);
			break;
		case 16:
			*old_val = *(u16 *)buf;
			handle_dynval(pid, cfg_en, *(u16 *)buf, (u16 *) &cfg_en->value, mem_offs);
			change_mem_val(pid, cfg_en, (u16) cfg_en->value, buf, mem_offs);
			break;
		default:
			*old_val = *(u8 *)buf;
			handle_dynval(pid, cfg_en, *(u8 *)buf, (u8 *) &cfg_en->value, mem_offs);
			change_mem_val(pid, cfg_en, (u8) cfg_en->value, buf, mem_offs);
			break;
		}
	}
}

/*
 * The function run_pgrp_bg() is so hacky OS security
 * bypassing so that it is not possible to wait for the
 * child process (the game) in a regular way. We have
 * to do that here in an equal hacky way as the process
 * belongs to init.
 */
static void catch_orphan (char *proc_name)
{
	pid_t pid;

	while (1) {
		pid = proc_to_pid(proc_name);
		if (pid < 0)
			return;
		sleep_sec(1);
	}
}

static i32 run_game (struct app_options *opt)
{
	pid_t pid = -1;
	const char *cmd, *pcmd;
	char *cmdv[2], *pcmdv[4];
	char pid_str[12] = { '\0' };
	string cmd_str = string("");

	if (!opt->pre_cmd) {
		cmd = (const char *) opt->game_path;

		cmdv[0] = opt->game_path;
		cmdv[1] = NULL;

		if (opt->run_scanmem) {
			restore_getch();

			pcmd = (const char *) SCANMEM;
			pcmdv[0] = (char *) SCANMEM;
			pcmdv[1] = (char *) "-p";
			pcmdv[2] = pid_str;
			pcmdv[3] = NULL;

			cout << "$ " << pcmdv[0] << " " << pcmdv[1]
			     << " `pidof -s " << opt->proc_name << "` & --> "
			     << "$ " << cmdv[0] << " &" << endl;

			pid = run_pgrp_bg(pcmd, pcmdv, cmd, cmdv,
					  pid_str, opt->proc_name, 3,
					  false, false);
			if (pid > 0)
				opt->scanmem_pid = pid;
		} else {
			cout << "$ " << cmdv[0] << " &" << endl;

			pid = run_cmd_bg(cmd, cmdv, false, false);
		}
	} else {
		if (opt->use_glc) {
			cmd_str += GLC_PRELOADER;
			cmd_str += " ";
		}
		cmd_str += opt->pre_cmd;
		cmd_str += " ";
		cmd_str += opt->game_path;
		cmd = cmd_str.c_str();

		if (opt->run_scanmem) {
			restore_getch();

			pcmd = (const char *) SCANMEM;
			pcmdv[0] = (char *) SCANMEM;
			pcmdv[1] = (char *) "-p";
			pcmdv[2] = pid_str;
			pcmdv[3] = NULL;

			cout << "$ " << pcmdv[0] << " " << pcmdv[1]
			     << " `pidof -s " << opt->proc_name << "` & --> "
			     << "$ " << cmd << " &" << endl;

			pid = run_pgrp_bg(pcmd, pcmdv, cmd, cmdv,
					  pid_str, opt->proc_name, 3,
					  false, true);
			if (pid > 0)
				opt->scanmem_pid = pid;
		} else {
			cout << "$ " << cmd << " &" << endl;

			pid = run_cmd_bg(cmd, cmdv, false, true);
		}
	}
	if (pid < 0)
		goto err;

	return 0;
err:
	cerr << "Error while running the game!" << endl;
	return -1;
}

#ifdef __linux__
static i32 run_preloader (struct app_options *opt)
{
	pid_t pid = -1;
	const char *cmd, *pcmd;
	char *cmdv[4], *pcmdv[4];
	char pid_str[12] = { '\0' };
	string cmd_str = string("");

	if (!opt->pre_cmd) {
		cmd = (const char *) PRELOADER;

		cmdv[0] = (char *) PRELOADER;
		cmdv[1] = opt->preload_lib;
		cmdv[2] = opt->game_path;
		cmdv[3] = NULL;

		if (opt->run_scanmem &&
		    (!opt->disc_str || opt->disc_str[0] != '5')) {
			restore_getch();

			pcmd = (const char *) SCANMEM;
			pcmdv[0] = (char *) SCANMEM;
			pcmdv[1] = (char *) "-p";
			pcmdv[2] = pid_str;
			pcmdv[3] = NULL;

			cout << "$ " << pcmdv[0] << " " << pcmdv[1]
			     << " `pidof -s " << opt->proc_name << "` & --> "
			     << "$ " << cmdv[0] << " " << cmdv[1]
			     << " " << cmdv[2] <<" &" << endl;

			pid = run_pgrp_bg(pcmd, pcmdv, cmd, cmdv,
					  pid_str, opt->proc_name, 3,
					  false, false);
			if (pid > 0)
				opt->scanmem_pid = pid;
		} else {
			cout << "$ " << cmdv[0] << " " << cmdv[1]
			     << " " << cmdv[2] << " &" << endl;

			pid = run_cmd_bg(cmd, cmdv, false, false);
		}
	} else {
		if (opt->use_glc) {
			cmd_str += GLC_PRELOADER;
			cmd_str += " ";
			cmd_str += opt->pre_cmd;
			cmd_str += " --preload=";
			cmd_str += opt->preload_lib;
		} else {
			cmd_str += opt->pre_cmd;
		}
		cmd_str += " ";
		cmd_str += opt->game_path;
		cmd = cmd_str.c_str();

		if (opt->run_scanmem &&
		    (!opt->disc_str || opt->disc_str[0] != '5')) {
			restore_getch();

			pcmd = (const char *) SCANMEM;
			pcmdv[0] = (char *) SCANMEM;
			pcmdv[1] = (char *) "-p";
			pcmdv[2] = pid_str;
			pcmdv[3] = NULL;

			cout << "$ " << pcmdv[0] << " " << pcmdv[1]
			     << " `pidof -s " << opt->proc_name << "` & --> "
			     << "$ " << cmd << " &" << endl;

			pid = run_pgrp_bg(pcmd, pcmdv, cmd, cmdv,
					  pid_str, opt->proc_name, 3,
					  false, true);
			if (pid > 0)
				opt->scanmem_pid = pid;
		} else {
			cout << "$ " << cmd << " &" << endl;

			pid = run_cmd_bg(cmd, cmdv, false, true);
		}
	}
	if (pid < 0)
		goto err;

	return 0;
err:
	cerr << "Error while running preloader!" << endl;
	return -1;
}
#endif

static i32 prepare_dynmem (struct app_options *opt, list<CfgEntry> *cfg,
			   i32 *ifd, i32 *ofd)
{
	char obuf[PIPE_BUF] = { 0 };
	u32 num_cfg = 0, num_cfg_len = 0, pos = 0;
	void *old_code_addr = NULL;
	list<CfgEntry>::iterator it;
#ifdef __linux__
	size_t written;
#endif

	// check for discovery first
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

	// fill the output buffer with the dynmem cfg
	for (it = cfg->begin(); it != cfg->end(); it++) {
		if (it->dynmem && it->dynmem->code_addr != old_code_addr) {
			num_cfg++;
			pos += snprintf(obuf + pos, sizeof(obuf) - pos,
				";%lu;%p", (ulong) it->dynmem->mem_size,
				it->dynmem->code_addr);
				pos += snprintf(obuf + pos, sizeof(obuf) - pos,
					";%p", it->dynmem->stack_offs);
			old_code_addr = it->dynmem->code_addr;
		}
	}
	// put the number of cfgs to the end
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
		return 0;

skip_memhack:
#ifdef __linux__
	// remove FIFOs first for empty FIFOs
	if ((unlink(DYNMEM_IN) && errno != ENOENT) ||
	    (unlink(DYNMEM_OUT) && errno != ENOENT)) {
		perror("unlink FIFO");
		return 1;
	}

	// set up and open FIFOs
	if (mkfifo(DYNMEM_IN, S_IRUSR | S_IWUSR |
	    S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0 && errno != EEXIST) {
		perror("input mkfifo");
		return 1;
	}
	/* Bug in Ubuntu: mkfifo ignores mode */
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
	/* Bug in Ubuntu: mkfifo ignores mode */
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

	/* Run the preloaded game but not as root */
	if (opt->preload_lib && getuid() != 0) {
		cout << "Starting preloaded game.." << endl;
		run_preloader(opt);
	}

	cout << "Waiting for preloaded game.." << endl;
	*ofd = open(DYNMEM_OUT, O_WRONLY | O_TRUNC);
	if (*ofd < 0) {
		perror("open ofd");
		return 1;
	}

	// write dynmem cfg to output FIFO
	written = write(*ofd, obuf, pos);
	if (written < pos) {
		perror("FIFO write");
		return 1;
	}
#endif
	return 0;
}

static i32 parse_adapt_result (struct app_options *opt, list<CfgEntry> *cfg,
			       char *buf, ssize_t buf_len)
{
	char *part_end = buf;
	ssize_t part_size, ppos = 0;
	u32 i, num_obj = 0;
	string *obj_name = NULL;
	void *code_addr = NULL;
	list<CfgEntry>::iterator it;
	DynMemEntry *tmp = NULL;
	int found;

	part_end = strchr(buf, ';');
	if (part_end == NULL)
		goto parse_err;
	if (sscanf(buf, "%u", &num_obj) != 1)
		goto parse_err;
	part_size = part_end - buf;
	ppos += part_size + 1;

	for (i = 1; i <= num_obj; i++) {
		part_end = strchr(buf + ppos, ';');
		if (part_end == NULL)
			goto parse_err;
		part_size = part_end - (buf + ppos);
		obj_name = new string(buf + ppos, part_size);
		ppos += part_size + 1;

		if (sscanf(buf + ppos, "%p", &code_addr) != 1)
			goto parse_err;

		// find object and set adp_addr
		found = 0;
		for (it = cfg->begin(); it != cfg->end(); it++) {
			tmp = it->dynmem;
			if (tmp && !tmp->adp_addr &&
			    tmp->name == *obj_name) {
				tmp->adp_addr = code_addr;
				cout << "Class " << tmp->name
				     << ", old_code: " << hex << tmp->code_addr
				     << ", new_code: " << tmp->adp_addr
				     << dec << endl;
				found = 1;
				break;
			}
		}
		if (!found)
			goto parse_err;
		if (i == num_obj)
			break;

		part_end = strchr(buf + ppos, ';');
		if (part_end == NULL)
			goto parse_err;
		part_size = part_end - (buf + ppos);
		ppos += part_size + 1;
	}

	return 0;
parse_err:
	cerr << "Error while parsing adaption output!" << endl;
	if (buf[buf_len - 1] == '\n')
		cerr << "-->" << (buf + ppos);
	else
		cerr << "-->" << (buf + ppos) << endl;
	return -1;
}

static i32 adapt_config (struct app_options *opt, list<CfgEntry> *cfg)
{
	char pbuf[PIPE_BUF] = { 0 };
	ssize_t read_bytes;
	const char *cmd = (const char *) opt->adp_script;
	char *cmdv[] = {
		opt->adp_script,
		opt->game_path,
		NULL
	};

#ifdef __linux__
	if (getuid() == 0)
		goto err;
#endif

	read_bytes = run_cmd_pipe(cmd, cmdv, pbuf, sizeof(pbuf), false);
	if (read_bytes <= 0)
		goto err;
	cout << "Adaption return:" << endl;
	if (pbuf[read_bytes - 1] == '\n')
		cout << pbuf;
	else
		cout << pbuf << endl;

	if (parse_adapt_result(opt, cfg, pbuf, read_bytes) != 0)
		goto err;

	return 0;
err:
	cerr << "Error while running adaption script!" << endl;
	return -1;
}

// mf() callback for read_dynmem_buf()
void set_dynmem_addr (list<CfgEntry> *cfg,
		      struct post_parse *pp,
		      void *heap_start,
		      void *mem_addr,
		      size_t mem_size,
		      void *code_addr,
		      void *stack_offs)
{
	list<CfgEntry>::iterator it;
	vector<void *> *mvec;

	//cout << "m" << hex << mem_addr << ";c"
	//	<< code_addr << dec << endl;

	// find object and set mem_addr
	for (it = cfg->begin(); it != cfg->end(); it++) {
		if (it->dynmem &&
		    it->dynmem->code_addr == code_addr) {
			mvec = &it->dynmem->v_maddr;
			mvec->push_back(mem_addr);
			it->dynmem->num_alloc++;
			/*cout << "Obj. " << it->dynmem->name << "; c"
				<< it->dynmem->code_addr << "; s"
				<< it->dynmem->mem_size << "; created at "
				<< mvec->back() << "; now: " << mvec->size()
				<< endl;*/
			break;
		}
	}
}

i32 find_addr_idx(vector<void *> *vec, void *addr)
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
void unset_dynmem_addr (list<CfgEntry> *cfg, void *argp, void *mem_addr)
{
	list<CfgEntry>::iterator it;
	vector<void *> *mvec;
	i32 idx;

	//cout << "f" << hex << mem_addr << dec << endl;
	for (it = cfg->begin(); it != cfg->end(); it++) {
		if (it->dynmem) {
			mvec = &it->dynmem->v_maddr;
			idx = find_addr_idx(mvec, mem_addr);
			if (idx < 0)
				continue;
			mvec->erase(mvec->begin() + idx);
			it->dynmem->num_freed++;

			/*cout << "Obj. " << it->dynmem->name << "; c"
				<< it->dynmem->code_addr << "; s"
				<< it->dynmem->mem_size << "; freed from "
				<< mem_addr << "; remaining: " << mvec->size()
				<< endl;*/
			break;
		}
	}
}

i32 main (i32 argc, char **argv, char **env)
{
	string input_str, *cfg_path = NULL;
	vector<string> lines;
	list<CfgEntry> cfg;
	list<CfgEntry>::iterator cfg_it;
	list<CfgEntry*> *cfg_act = NULL;
	list<CfgEntry*>::iterator it;
	list<CfgEntry*> *cfgp_map[256] = { NULL };
	CfgEntry *cfg_en;
	DynMemEntry *old_dynmem = NULL;
	vector<void *> *mvec;
	void *mem_addr, *mem_offs = NULL;
	pid_t pid, worker_pid;
	char def_home[] = "~";
	u8 buf[sizeof(i64)] = { 0 };
	i32 ret, pmask = PARSE_M | PARSE_C;
	char ch;
	i32 ifd = -1, ofd = -1;
	struct app_options opt;
	bool emptycfg = false;
	u32 mem_idx, ov_idx, num_kicked;
	bool is_dynmem;

	atexit(restore_getch);

	if (argc < 2)
		usage();

	parse_options(argc, argv, &opt);

	opt.home = getenv(HOME_VAR);
	if (!opt.home)
		opt.home = def_home;

	if (strncmp(argv[optind - 1], "NONE", sizeof("NONE") - 1) != 0) {
		cfg_path = new string(argv[optind - 1]);
		cfg_act = read_config(cfg_path, &opt, &cfg, cfgp_map, &lines);
		cout << "Found config for \"" << opt.proc_name << "\"." << endl;
	} else {
		cfg_path = new string("NONE");
		if (!(opt.disc_str || opt.run_scanmem) ||
		    (opt.disc_str[0] < '0' || opt.disc_str[0] > '4')) {
			cerr << "Error: Config required!" << endl;
			return -1;
		}

		cout << "Process name: ";
		fflush(stdout);
		cin >> input_str;
		opt.proc_name = to_c_str(&input_str);
	}

	if (opt.disc_str) {
		if (opt.disc_str[0] >= '0' && opt.disc_str[0] <= '4') {
			cout << "Clearing config for discovery!" << endl;
			cfg.clear();
			cfg_act->clear();
			emptycfg = true;
		} else {
			opt.run_scanmem = false;
		}
	} else if (opt.run_scanmem) {
		cout << "Clearing config for scanmem!" << endl;
		cfg.clear();
		cfg_act->clear();
		emptycfg = true;
	}

	if (!opt.game_path)
		opt.game_path = get_abs_app_path(opt.proc_name);
	if (!opt.game_path)
		return -1;

	cout << "Config:" << endl;
	output_config(&cfg);
	cout << "Activated:" << endl;
	output_configp(cfg_act);
	if (cfg.empty() && !emptycfg)
		return -1;

	if (prepare_getch() != 0) {
		cerr << "Error while terminal preparation!" << endl;
		return -1;
	}

	if (opt.adp_required && !opt.do_adapt && !opt.disc_str &&
	    !opt.run_scanmem) {
		if (!opt.adp_script) {
			cerr << "Error, adaption required but no adaption script!" << endl;
			return -1;
		}
		cout << "Adaption to your compiler/game version is required." << endl;
		cout << "Adaption script: " << opt.adp_script << endl;
		cout << "Run the adaption script, now (y/n)? : ";
		fflush(stdout);
		ch = 'n';
		ch = do_getch();
		cout << ch << endl;
		if (ch == 'y') {
			opt.do_adapt = true;
			do_assumptions(&opt);
		}
	}

	if (opt.do_adapt) {
		if (!opt.adp_script) {
			cerr << "Error, no adaption script!" << endl;
			return -1;
		}
		if (adapt_config(&opt, &cfg) != 0) {
			cerr << "Error while code address adaption!" << endl;
			return -1;
		}
		if (opt.use_gbt) {
			take_over_config(&opt, &cfg, cfg_path, &lines);
		} else {
			cout << "Adapt reverse stack offset(s) (y/n)? : ";
			fflush(stdout);
			ch = 'n';
			ch = do_getch();
			cout << ch << endl;
			if (ch != 'y')
				take_over_config(&opt, &cfg, cfg_path, &lines);
		}
	}

discover_next:
	if (prepare_discovery(&opt, &cfg) != 0)
		return -1;

prepare_dynmem:
	if (prepare_dynmem(&opt, &cfg, &ifd, &ofd) != 0) {
		cerr << "Error while dyn. mem. preparation!" << endl;
		return -1;
	}

	pid = proc_to_pid(opt.proc_name);
	if (pid < 0) {
		/* Run the game but not as root */
		if (opt.preload_lib) {
#ifdef __linux__
			if (getuid() == 0)
				return -1;
#endif
			cout << "Starting the game.." << endl;
			run_game(&opt);
			sleep_sec(1);
			pid = proc_to_pid(opt.proc_name);
			if (pid < 0)
				goto pid_err;
		} else {
			goto pid_err;
		}
	}
	cout << "PID: " << pid << endl;

	if (opt.disc_str) {
		pmask = PARSE_M | PARSE_S | PARSE_C | PARSE_O;
		if (opt.disc_str[0] == '0') {
			if (opt.scanmem_pid > 0) {
				wait_proc(opt.scanmem_pid);
				// Have you closed scanmem before the game?
				catch_orphan(opt.proc_name);
			} else {
				wait_proc(pid);
			}
			return 0;
		} else if (opt.disc_str[0] >= '1' && opt.disc_str[0] <= '4') {
			worker_pid = fork_proc(run_stage1234_loop, &ifd);
			if (opt.scanmem_pid > 0) {
				wait_proc(opt.scanmem_pid);
				// Have you closed scanmem before the game?
				catch_orphan(opt.proc_name);
			} else {
				wait_proc(pid);
			}
			kill_proc(worker_pid);
			if (worker_pid < 0)
				return -1;
		} else if (opt.disc_str[0] == '5') {
			run_stage5_loop(&cfg, ifd, pmask, pid);
		}
		ret = postproc_discovery(&opt, &cfg, cfg_path, &lines);
		switch (ret) {
		case DISC_NEXT:
			goto discover_next;
			break;
		case DISC_OKAY:
			pmask = PARSE_M | PARSE_C;
			goto prepare_dynmem;
			break;
		default:
			break;
		}
	} else if (opt.scanmem_pid > 0) {
		wait_proc(opt.scanmem_pid);
		// Have you closed scanmem before the game?
		catch_orphan(opt.proc_name);
		return 0;
	}

	if (opt.do_adapt || opt.disc_str || opt.run_scanmem)
		return -1;

	set_getch_nb(1);

	if (memattach_test(pid) != 0) {
		cerr << "PTRACE ERROR PID[" << pid << "]!" << endl;
		return -1;
	}

	while (1) {
		sleep_sec(1);
		ch = do_getch();
		if (ch > 0) {
			if (cfgp_map[(i32)ch])
				toggle_cfg(cfgp_map[(i32)ch], cfg_act);
			else if (ch == '>')
				dump_all_mem_obj(pid, &cfg);
			else if (ch == '+')
				inc_dec_mvec_pridx(&cfg, true);
			else if (ch == '-')
				inc_dec_mvec_pridx(&cfg, false);
		}

		// get allocated and freed objects (TIME CRITICAL!)
		read_dynmem_buf(&cfg, NULL, ifd, pmask, false,
				set_dynmem_addr, unset_dynmem_addr);

		// print allocated and freed object counts
		old_dynmem = NULL;
		for (cfg_it = cfg.begin(); cfg_it != cfg.end(); cfg_it++) {
			if (cfg_it->dynmem && cfg_it->dynmem != old_dynmem) {
				mvec = &cfg_it->dynmem->v_maddr;
				if (cfg_it->dynmem->num_alloc > 0)
					cout << "===> Obj. " << cfg_it->dynmem->name
					     << " created " << cfg_it->dynmem->num_alloc
					     << " time(s); now: " << mvec->size() << endl;
				if (cfg_it->dynmem->num_freed > 0)
					cout << "===> Obj. " << cfg_it->dynmem->name
					     << " freed " << cfg_it->dynmem->num_freed
					     << " time(s); remaining: " << mvec->size() << endl;
				cfg_it->dynmem->num_alloc = 0;
				cfg_it->dynmem->num_freed = 0;
				old_dynmem = cfg_it->dynmem;
			}
		}

		// allocate old values per memory object
		for (it = cfg_act->begin(); it != cfg_act->end(); it++) {
			cfg_en = *it;
			if (cfg_en->dynmem) {
				for (mem_idx = 0;
				     mem_idx < cfg_en->dynmem->v_maddr.size();
				     mem_idx++) {
					if (mem_idx >= cfg_en->v_oldval.size())
						cfg_en->v_oldval.push_back(0);
				}
			}
		}

		if (cfg_act->empty())
			goto skip_inactive;

		if (memattach(pid) != 0) {
			cerr << "PTRACE ATTACH ERROR PID[" << pid << "]!" << endl;
			return -1;
		}

		// TIME CRITICAL! Process all activated config entries
		for (it = cfg_act->begin(); it != cfg_act->end(); it++) {
			cfg_en = *it;
			if (cfg_en->dynmem) {
				for (mem_idx = 0;
				     mem_idx < cfg_en->dynmem->v_maddr.size();
				     mem_idx++) {
					mem_offs = cfg_en->dynmem->v_maddr[mem_idx];
					if (mem_offs == NULL)
						continue;
					cfg_en->dynmem->objidx = mem_idx;

					mem_addr = PTR_ADD(void *, mem_offs, cfg_en->addr);
					if (memread(pid, mem_addr, buf, sizeof(i64)) != 0) {
						cerr << "PTRACE READ MEMORY ERROR PID["
						     << pid << "]!" << endl;
						return -1;
					}
					change_memory(pid, cfg_en, buf, mem_offs,
						&cfg_en->v_oldval[mem_idx]);
				}
			} else {
				mem_offs = NULL;

				mem_addr = cfg_en->addr;
				if (memread(pid, mem_addr, buf, sizeof(i64)) != 0) {
					cerr << "PTRACE READ MEMORY ERROR PID["
					     << pid << "]!" << endl;
					return -1;
				}
				change_memory(pid, cfg_en, buf, mem_offs, &cfg_en->old_val);
			}
		}

		if (memdetach(pid) != 0) {
			cerr << "PTRACE DETACH ERROR PID[" << pid << "]!" << endl;
			return -1;
		}

skip_inactive:
		// remove old values marked to be removed by object check
		for (it = cfg_act->begin(); it != cfg_act->end(); it++) {
			cfg_en = *it;
			if (cfg_en->dynmem) {
				mvec = &cfg_en->dynmem->v_maddr;
				for (mem_idx = 0, ov_idx = 0;
				     mem_idx < mvec->size() &&
				     ov_idx < cfg_en->v_oldval.size();
				     mem_idx++, ov_idx++) {
					if (!mvec->at(mem_idx)) {
						cfg_en->v_oldval.erase(cfg_en->v_oldval.begin()
							+ ov_idx);
						ov_idx--;
					}
				}
			}
		}

		// remove objects marked to be removed by object check
		old_dynmem = NULL;
		for (it = cfg_act->begin(); it != cfg_act->end(); it++) {
			cfg_en = *it;
			if (cfg_en->dynmem && cfg_en->dynmem != old_dynmem) {
				mvec = &cfg_en->dynmem->v_maddr;
				num_kicked = 0;
				for (mem_idx = 0; mem_idx < mvec->size(); mem_idx++) {
					if (!mvec->at(mem_idx)) {
						mvec->erase(mvec->begin() + mem_idx);
						num_kicked++;
						mem_idx--;
					}
				}
				if (num_kicked > 0)
					cout << "===> Obj. " << cfg_en->dynmem->name
					     << " kicked out " << num_kicked
					     << " time(s); remaining: " << mvec->size() << endl;
				old_dynmem = cfg_en->dynmem;
			}
		}

		// output old values
		old_dynmem = NULL;
		for (it = cfg_act->begin(); it != cfg_act->end(); it++) {
			cfg_en = *it;
			is_dynmem = false;
			if (cfg_en->dynmem) {
				mvec = &cfg_en->dynmem->v_maddr;
				if (mvec->empty()) {
					continue;
				} else {
					if (cfg_en->dynmem->pridx >= mvec->size())
						cfg_en->dynmem->pridx = mvec->size() - 1;
					mem_offs = mvec->at(cfg_en->dynmem->pridx);
					cfg_en->old_val =
						cfg_en->v_oldval[cfg_en->dynmem->pridx];
					is_dynmem = true;
					if (cfg_en->dynmem != old_dynmem) {
						cout << "*" << cfg_en->dynmem->name << "["
						     << cfg_en->dynmem->pridx << "]" << " = "
						     << hex << mem_offs << dec << ", "
						     << mvec->size() << " obj." << endl;
							old_dynmem = cfg_en->dynmem;
					}
				}
			} else {
				mem_offs = NULL;
			}
			output_mem_val(cfg_en, mem_offs, is_dynmem);
		}

	}

	return 0;

pid_err:
	cerr << "PID not found or invalid!" << endl;
	return -1;
}
