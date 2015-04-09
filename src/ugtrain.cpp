/* ugtrain.cpp:    freeze values in process memory (game trainer)
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * powered by the Open Game Cheating Association
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

#include <cstring>
#include <fstream>
#include <list>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <libgen.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

// local includes
#include <common.h>
// input
#include <lib/getch.h>
#include <cfgentry.h>
#include <cfgparser.h>
#include <fifoparser.h>
#include <options.h>
// processing
#include <lib/memattach.h>
#include <lib/preload.h>
#include <lib/system.h>
#include <control.h>
#include <memmgmt.h>
// output
#include <cfgoutput.h>
#include <valoutput.h>
// special features
#include <adaption.h>
#include <aslr.h>
#include <discovery.h>
#include <testing.h>

#define HOME_VAR   "HOME"
#define DYNMEM_IN  "/tmp/memhack_out"
#define DYNMEM_OUT "/tmp/memhack_in"
#define MEMDISC_IN "/tmp/memdisc_out"


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
		if (vvec->size() <= 0)
			goto err;
		*buf = vvec->at(dynmem->obj_idx);
	} else if (ptrmem && ptrmem->dynmem) {
		DynMemEntry *dynmem = ptrmem->dynmem;
		vector<value_t> *vvec = &cfg_ref->v_oldval;
		if (vvec->size() <= 0)
			goto err;
		*buf = vvec->at(dynmem->obj_idx);
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
		} else if (chk_en->check_obj_num) {
			if (!dynmem)
				continue;
			chk_buf->u32 = dynmem->obj_idx;
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
	value_t *value = &cfg_en->value;

	if (type->is_float) {
		switch (type->size) {
		case 64:
			old_val->f64 = buf->f64;
			handle_dynval(pid, cfg_en, old_val->f64, &value->f64, mem_offs);
			change_mem_val(pid, cfg_en, old_val->f64, value->f64, buf, mem_offs);
			break;
		case 32:
			old_val->f32 = buf->f32;
			handle_dynval(pid, cfg_en, old_val->f32, &value->f32, mem_offs);
			change_mem_val(pid, cfg_en, old_val->f32, value->f32, buf, mem_offs);
			break;
		}
	} else if (type->is_signed) {
		switch (type->size) {
		case 64:
			old_val->i64 = buf->i64;
			handle_dynval(pid, cfg_en, old_val->i64, &value->i64, mem_offs);
			change_mem_val(pid, cfg_en, old_val->i64, value->i64, buf, mem_offs);
			break;
		case 32:
			old_val->i32 = buf->i32;
			handle_dynval(pid, cfg_en, old_val->i32, &value->i32, mem_offs);
			change_mem_val(pid, cfg_en, old_val->i32, value->i32, buf, mem_offs);
			break;
		case 16:
			old_val->i16 = buf->i16;
			handle_dynval(pid, cfg_en, old_val->i16, &value->i16, mem_offs);
			change_mem_val(pid, cfg_en, old_val->i16, value->i16, buf, mem_offs);
			break;
		default:
			old_val->i8 = buf->i8;
			handle_dynval(pid, cfg_en, old_val->i8, &value->i8, mem_offs);
			change_mem_val(pid, cfg_en, old_val->i8, value->i8, buf, mem_offs);
			break;
		}
	} else {
		switch (type->size) {
		case 64:
			old_val->u64 = buf->u64;
			handle_dynval(pid, cfg_en, old_val->u64, &value->u64, mem_offs);
			change_mem_val(pid, cfg_en, old_val->u64, value->u64, buf, mem_offs);
			break;
		case 32:
			old_val->u32 = buf->u32;
			handle_dynval(pid, cfg_en, old_val->u32, &value->u32, mem_offs);
			change_mem_val(pid, cfg_en, old_val->u32, value->u32, buf, mem_offs);
			break;
		case 16:
			old_val->u16 = buf->u16;
			handle_dynval(pid, cfg_en, old_val->u16, &value->u16, mem_offs);
			change_mem_val(pid, cfg_en, old_val->u16, value->u16, buf, mem_offs);
			break;
		default:
			old_val->u8 = buf->u8;
			handle_dynval(pid, cfg_en, old_val->u8, &value->u8, mem_offs);
			change_mem_val(pid, cfg_en, old_val->u8, value->u8, buf, mem_offs);
			break;
		}
	}
}

// TIME CRITICAL! Process all activated config entries from pointer
static void process_ptrmem (pid_t pid, CfgEntry *cfg_en, value_t *buf, u32 mem_idx)
{
	ptr_t mem_addr;
	PtrMemEntry *ptrtgt = cfg_en->ptrtgt;
	list<CfgEntry*> *cfg_act = &ptrtgt->cfg_act;
	list<CfgEntry*>::iterator it;
	vector<value_t> *vvec = &cfg_en->v_oldval;
	value_t *value = &vvec->at(mem_idx);

	if (buf->ptr == 0 || ptrtgt->v_state[mem_idx] == PTR_DONE)
		return;

	if (buf->ptr == value->ptr) {
		if (cfg_en->dynval == DYN_VAL_PTR_ONCE)
			ptrtgt->v_state[mem_idx] = PTR_DONE;
		else
			ptrtgt->v_state[mem_idx] = PTR_SETTLED;
		ptrtgt->v_offs[mem_idx] = buf->ptr;
		list_for_each (cfg_act, it) {
			PtrMemEntry *ptrmem;
			cfg_en = *it;
			ptrmem = cfg_en->ptrmem;
			mem_addr = ptrmem->v_offs[mem_idx] + cfg_en->addr;
			if (read_memory(pid, mem_addr, buf, "PTR MEMORY"))
				continue;
			change_memory(pid, cfg_en, buf, ptrmem->v_offs[mem_idx],
				      &cfg_en->v_oldval[mem_idx]);
		}
	} else {
		value->ptr = buf->ptr;
	}
}

// TIME CRITICAL! Read the memory allocations and freeings from the input FIFO.
// If the buffer fills, the game process hangs.
static inline void read_dynmem_fifo (list<CfgEntry> *cfg, i32 ifd, i32 pmask)
{
	ssize_t rbytes;
	struct parse_cb pcb = { NULL };

	pcb.mf = alloc_dynmem_addr;
	pcb.ff = clear_dynmem_addr;

	do {
		rbytes = read_dynmem_buf(cfg, NULL, ifd, pmask, false,
			0, &pcb);
	} while (rbytes > 0);
}

// TIME CRITICAL! Process all activated config entries.
// Note: We are attached to the game process and it is frozen.
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
			DynMemEntry *dynmem = cfg_en->dynmem;
			vector<ptr_t> *mvec = &dynmem->v_maddr;
			struct type *type = &cfg_en->type;
			for (mem_idx = 0;
			     mem_idx < mvec->size();
			     mem_idx++) {
				size_t mem_size;
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

				mem_addr = mem_offs + cfg_en->addr;
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

static void reset_terminal (void)
{
	const char *cmd = "reset";
	char *cmdv[2] = { (char *) "reset", NULL };

	restore_getch();
	run_cmd(cmd, cmdv);
	prepare_getch_nb();
}

static pid_t run_game (struct app_options *opt, char *preload_lib)
{
	pid_t pid = -1;
	pid_t old_game_pid = proc_to_pid(opt->proc_name);
	pid_t game_pid;
	u32 i, pos;
	enum pstate pstate;
	string cmd_str, game_path;
	const char *cmd;
	char prev_ch;

	if (opt->pre_cmd) {
		if (opt->use_glc) {
			cmd_str += GLC_PRELOADER;
			cmd_str += " ";
		}
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

	if (opt->game_params) {
		cmd_str += " ";
		cmd_str += opt->game_params;
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
		sigignore(SIGINT);

		cout << "$ " << pcmdv[0] << " " << pcmdv[1]
		     << " `pidof " << opt->proc_name
		     << " | cut -d \' \' -f 1` & --> "
		     << "$ " << cmd_str << " &" << endl;

		cmd = cmd_str.c_str();
		pid = run_pgrp_bg(pcmd, pcmdv, cmd, NULL,
				  pid_str, opt->proc_name, 3,
				  false, preload_lib);
		if (pid > 0)
			opt->scanmem_pid = pid;
	} else {
		cout << "$ " << cmd_str << " &" << endl;

		cmd = cmd_str.c_str();
		pid = run_cmd_bg(cmd, NULL, false, preload_lib);
	}
	if (pid < 0)
		goto err;

	for (i = 0; i < 100; i++) {
		sleep_msec(50);
		pstate = check_process(pid, opt->proc_name);
		if (pstate == PROC_RUNNING)
			break;
		// handling of a loader which may end very late after forking
		game_pid = proc_to_pid(opt->proc_name);
		if (game_pid != old_game_pid)
			pid = game_pid;
	}
	return pid;
err:
	cerr << "Error while running the game!" << endl;
	return -1;
}

#ifdef __linux__
static pid_t run_preloader (struct app_options *opt)
{
	pid_t pid;

	setenv(UGT_GAME_PROC_NAME, opt->proc_name, 1);

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
static i32 prepare_dynmem (struct app_options *opt, list<CfgEntry> *cfg,
			   i32 *ifd, i32 *ofd, i32 *dfd, pid_t *pid)
{
	char obuf[PIPE_BUF] = { 0 };
	u32 num_cfg = 0, num_cfg_len = 0, pos = 0;
	ptr_t old_code_addr = 0, old_grow_addr = 0;
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
		if ((!grow && (dynmem->code_addr != old_code_addr)) ||
		    (grow && (grow->code_addr != old_grow_addr))) {
			num_cfg++;
			pos += snprintf(obuf + pos, sizeof(obuf) - pos,
				";%lu;" SCN_PTR ";" SCN_PTR, (ulong)
				dynmem->mem_size, dynmem->code_addr,
				dynmem->stack_offs);
			if (dynmem->lib)
				pos += snprintf(obuf + pos, sizeof(obuf) - pos,
					";%s", dynmem->lib);
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
						";%s", grow->lib);
				else
					pos += snprintf(obuf + pos,
						sizeof(obuf) - pos, ";exe");
				old_grow_addr = grow->code_addr;
			} else {
				old_code_addr = dynmem->code_addr;
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
	if (opt->disc_str) {
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
		cout << "Starting game with " << opt->preload_lib
		     << " preloaded.." << endl;
		*pid = run_preloader(opt);
		if (*pid <= 0)
			goto err;
	} else {
		goto err;
	}
	cout << "Waiting for preloaded library.." << endl;
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

static inline i32 get_game_paths (struct app_options *opt)
{
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
#define CFGP_MAP_SIZE 128
	list<CfgEntry*> *cfgp_map[CFGP_MAP_SIZE] = { NULL };
	string input_str;
	pid_t pid = -1;
	char def_home[] = "~";
	i32 ret, pmask = PARSE_S | PARSE_C;
	char ch;
	i32 ifd = -1, ofd = -1, dfd = -1;
	bool allow_empty_cfg = false;
	enum pstate pstate;
	list<struct region> rlist;

	atexit(restore_getch);

	parse_options(argc, argv, opt);
	test_optparsing(opt);

	opt->home = getenv(HOME_VAR);
	if (!opt->home)
		opt->home = def_home;

	read_config(opt, cfg, cfg_act, cfgp_map, lines);
	test_cfgparsing(opt, cfg, cfg_act, cfgp_map);
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

	get_game_paths(opt);

	cout << "Config:" << endl;
	output_config(cfg);
	cout << endl;
	cout << "Activated:" << endl;
	output_configp(cfg_act);
	cout << endl;

	if (cfg->empty() && !allow_empty_cfg)
		return -1;
	test_cfgoutput(opt);

	if (prepare_getch() != 0) {
		cerr << "Error while terminal preparation!" << endl;
		return -1;
	}

	ret = process_adaption(opt, cfg, lines);
	if (ret)
		return ret;

	ret = get_game_paths(opt);
	if (ret)
		return ret;

discover_next:
	if (prepare_discovery(opt, cfg) != 0)
		return -1;

prepare_dynmem:
	ret = prepare_dynmem(opt, cfg, &ifd, &ofd, &dfd, &pid);
	if (ret == 2) {
		opt->pure_statmem = true;
	} else if (ret != 0) {
		cerr << "Error while dyn. mem. preparation!" << endl;
		return -1;
	}

	pid = proc_to_pid(opt->proc_name);
	if (pid < 0 && (!opt->pure_statmem || !opt->preload_lib)) {
		goto pid_err;
	} else if (opt->pure_statmem && opt->preload_lib) {
		/* Run the game but not as root */
#ifdef __linux__
		if (getuid() == 0)
			return -1;
#endif
		cout << "Starting the game.." << endl;
		pid = run_game(opt, NULL);
		if (pid < 0)
			goto pid_err;
	}
	cout << "PID: " << pid << endl;

	if (opt->disc_str) {
		process_discovery(opt, cfg, ifd, dfd, ofd, pid, &rlist);
		//list_regions(&rlist);
		ret = postproc_discovery(opt, cfg, &rlist, lines);
		switch (ret) {
		case DISC_NEXT:
			goto discover_next;
			break;
		case DISC_OKAY:
			pmask = PARSE_S | PARSE_C;
			goto prepare_dynmem;
			break;
		case DISC_EXIT:
			break;
		default:
			return -1;
		}
	} else if (opt->scanmem_pid > 0) {
		wait_orphan(pid, opt->proc_name);
		wait_proc(opt->scanmem_pid);
		return 0;
	}

	if (opt->do_adapt || opt->disc_str || opt->run_scanmem)
		return -1;

	set_getch_nb(1);

	if (memattach_test(pid) != 0) {
		cerr << "MEMORY ATTACHING TEST ERROR PID[" << pid << "]!" << endl;
		return -1;
	}

	handle_pie(opt, cfg, ifd, ofd, pid, &rlist);
	handle_statmem_pie(opt->code_offs, cfg);

	// use sleep_sec_unless_input2() also for pure static memory
	if (ifd < 0)
		ifd = STDIN_FILENO;

	while (true) {
		sleep_sec_unless_input2(1, ifd, STDIN_FILENO);
		ch = do_getch();
		if (ch >= 0 && ch < CFGP_MAP_SIZE)
			handle_input_char(ch, cfgp_map, pid, cfg, cfg_act);

		// get allocated and freed objects (TIME CRITICAL!)
		if (!opt->pure_statmem) {
			read_dynmem_fifo(cfg, ifd, pmask);
			// print allocated and freed object counts
			ret = output_dynmem_changes(cfg);
			if (ret)
				reset_terminal();
		}

		// check for active config
		if (cfg_act->empty()) {
			pstate = check_process(pid, opt->proc_name);
			if (pstate != PROC_RUNNING && pstate != PROC_ERR)
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
			pstate = check_process(pid, opt->proc_name);
			if (pstate != PROC_RUNNING && pstate != PROC_ERR)
				return 0;
			// coming here often when endling the game
			memattach_err_once(pid);
			continue;
		}

		// read from the FIFO again
		// R/W to invalid heap addresses crashes the game (SIGSEGV).
		// There could have been free() calls before freezing the game.
		if (!opt->pure_statmem) {
			read_dynmem_fifo(cfg, ifd, pmask);
			// We might also read mallocs. Can't ignore them!
			alloc_dynmem(cfg);
		}
		// TIME CRITICAL! Process all activated config entries
		process_act_cfg(pid, cfg_act);

		if (memdetach(pid) != 0) {
			cerr << "MEMORY DETACH ERROR PID[" << pid << "]!" << endl;
			return -1;
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
	}

	return 0;
pid_err:
	cerr << "PID not found or invalid!" << endl;
	return -1;
}
