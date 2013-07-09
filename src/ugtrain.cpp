/* ugtrain.cpp:    freeze values in process memory (game trainer)
 *
 * Copyright (c) 2012..13, by:  Sebastian Riemer
 *    All rights reserved.      Ernst-Reinke-Str. 23
 *                              10369 Berlin, Germany
 *                             <sebastian.riemer@gmx.de>
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

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <vector>
#include <cstring>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/stat.h>

// local includes
#include "options.h"
#include "cfgentry.h"
#include "parser.h"
#include "getch.h"
#include "memattach.h"
using namespace std;

#define HOME_VAR   "HOME"
#define DYNMEM_IN  "/tmp/memhack_out"
#define DYNMEM_OUT "/tmp/memhack_in"

template <typename T>
string to_string (T val)
{
	ostringstream ss;

	ss << val;
	return ss.str();
}

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
			cout << " " << tmp_dval << endl;
		} else {
			cout << " " << it->value << endl;
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

static i32 run_cmd (const char *cmd, char *const cmdv[])
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		perror("fork");
		goto err;
	} else if (pid == 0) {
		if (execvp(cmd, cmdv) < 0) {
			perror("execvp");
			goto child_err;
		}
	}
	return 0;
err:
	return -1;
child_err:
	exit(-1);
}

static ssize_t run_cmd_pipe (const char *cmd, char *const cmdv[],
			     char *pbuf, size_t pbuf_size)
{
	pid_t pid;
	i32 status, fds[2];
	ssize_t bytes_read = 0;

	if (pipe(fds) < 0) {
		perror("pipe");
		goto err;
	}
	pid = fork();
	if (pid < 0) {
		perror("fork");
		close(fds[STDOUT_FILENO]);
		close(fds[STDIN_FILENO]);
		goto err;
	} else if (pid == 0) {
		if (dup2(fds[STDOUT_FILENO], STDOUT_FILENO) < 0) {
			perror("dup2");
			close(fds[STDOUT_FILENO]);
			close(fds[STDIN_FILENO]);
			goto child_err;
		}
		close(fds[STDIN_FILENO]);
		if (execvp(cmd, cmdv) < 0) {
			perror("execvp");
			close(fds[STDOUT_FILENO]);
			goto child_err;
		}
	} else {
		close(fds[STDOUT_FILENO]);
		waitpid(pid, &status, 0);
		bytes_read = read(fds[STDIN_FILENO], pbuf, pbuf_size);
		if (bytes_read < 0) {
			perror("pipe read");
			goto parent_err;
		} else if (bytes_read <= 1) {
			cerr << "The command did not return anything." << endl;
			goto parent_err;
		}
	}
	close(fds[STDIN_FILENO]);
	return bytes_read;

parent_err:
	close(fds[STDIN_FILENO]);
err:
	return -1;
child_err:
	exit(-1);
}

static pid_t proc_to_pid (char *proc_name)
{
	pid_t pid;
	char pbuf[PIPE_BUF] = {0};
	const char *cmd = (const char *) "pidof";
	char *cmdv[4];

	cmdv[0] = (char *) "pidof";
	cmdv[1] = (char *) "-s";
	cmdv[2] = proc_name;
	cmdv[3] = NULL;

	if (run_cmd_pipe(cmd, cmdv, pbuf, sizeof(pbuf)) <= 0)
		goto err;

	if (!isdigit(pbuf[0]))
		goto err;

	pid = atoi(pbuf);
	if (pid <= 1)
		goto err;

	return pid;
err:
	cerr << "PID not found or invalid!" << endl;
	return -1;
}

void toggle_cfg (list<CfgEntry*> *cfg, list<CfgEntry*> *cfg_act)
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
	double tmp_dval;

	if (cfg_en->is_float) {
		tmp_dval = (double) *(T *)buf;
		memcpy(&cfg_en->old_val, &tmp_dval, sizeof(i64));
	} else {
		cfg_en->old_val = *(T *)buf;
	}

	if (cfg_en->checks) {
		chk_lp = cfg_en->checks;
		list<CheckEntry>::iterator it;
		for (it = chk_lp->begin(); it != chk_lp->end(); it++) {
			mem_addr = PTR_ADD(void *, mem_offs, it->addr);

			if (memread(pid, mem_addr, chk_buf, sizeof(i64)) != 0) {
				cerr << "PTRACE READ MEMORY ERROR PID[" << pid << "]!" << endl;
				exit(-1);
			}
			if (check_memory(*it, chk_buf) != 0)
				return;
		}
	}

	if ((cfg_en->check == DO_LT && *(T *)buf < value) ||
	    (cfg_en->check == DO_GT && *(T *)buf > value)) {
		memcpy(buf, &value, sizeof(T));
		mem_addr = PTR_ADD(void *, mem_offs, cfg_en->addr);

		if (memwrite(pid, mem_addr, buf, sizeof(i64)) != 0) {
			cerr << "PTRACE WRITE MEMORY ERROR PID[" << pid << "]!" << endl;
			exit(-1);
		}
	}
}

static void change_memory (pid_t pid, CfgEntry *cfg_en, u8 *buf, void *mem_offs)
{
	double tmp_dval;

	if (cfg_en->is_float) {
		memcpy(&tmp_dval, &cfg_en->value, sizeof(i64));
		switch (cfg_en->size) {
		case 64:
			change_mem_val(pid, cfg_en, (double) tmp_dval, buf, mem_offs);
			break;
		case 32:
			change_mem_val(pid, cfg_en, (float) tmp_dval, buf, mem_offs);
			break;
		}
	} else if (cfg_en->is_signed) {
		switch (cfg_en->size) {
		case 64:
			change_mem_val(pid, cfg_en, (i64) cfg_en->value, buf, mem_offs);
			break;
		case 32:
			change_mem_val(pid, cfg_en, (i32) cfg_en->value, buf, mem_offs);
			break;
		case 16:
			change_mem_val(pid, cfg_en, (i16) cfg_en->value, buf, mem_offs);
			break;
		default:
			change_mem_val(pid, cfg_en, (i8) cfg_en->value, buf, mem_offs);
			break;
		}
	} else {
		switch (cfg_en->size) {
		case 64:
			change_mem_val(pid, cfg_en, (u64) cfg_en->value, buf, mem_offs);
			break;
		case 32:
			change_mem_val(pid, cfg_en, (u32) cfg_en->value, buf, mem_offs);
			break;
		case 16:
			change_mem_val(pid, cfg_en, (u16) cfg_en->value, buf, mem_offs);
			break;
		default:
			change_mem_val(pid, cfg_en, (u8) cfg_en->value, buf, mem_offs);
			break;
		}
	}
}

static i32 run_preloader (char *preload_lib, char *proc_name)
{
	i32 ret;
	const char *cmd = (const char *) PRELOADER;
	char *cmdv[4];

	cmdv[0] = (char *) PRELOADER;
	cmdv[1] = preload_lib;
	cmdv[2] = proc_name;
	cmdv[3] = NULL;

	cout << "$ " << cmdv[0] << " " << cmdv[1]
	     << " " << cmdv[2] << endl;

	ret = run_cmd(cmd, cmdv);
	if (ret)
		goto err;

	return ret;
err:
	cerr << "Error while running preloader!" << endl;
	return ret;
}

static i32 prepare_dynmem (struct app_options *opt, char *proc_name,
			   list<CfgEntry> *cfg, i32 *ifd, i32 *ofd)
{
	char obuf[4096] = {0};
	u32 num_cfg = 0, num_cfg_len = 0, pos = 0;
	size_t written;
	void *old_code_addr = NULL;
	list<CfgEntry>::iterator it;

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

	// fill the output buffer with the dynmem cfg
	for (it = cfg->begin(); it != cfg->end(); it++) {
		if (it->dynmem && it->dynmem->code_addr != old_code_addr) {
			num_cfg++;
			pos += snprintf(obuf + pos, sizeof(obuf) - pos,
				";%zd;%p;%p", it->dynmem->mem_size,
				it->dynmem->code_addr, it->dynmem->stack_offs);
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
		run_preloader(opt->preload_lib, proc_name);
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

	return 0;
}

static i32 prepare_discovery (struct app_options *opt, list<CfgEntry> *cfg)
{
	string disc_str;
	int i, found = 0;
	list<CfgEntry>::iterator it;

	if (!opt->disc_str)
		return 0;

	switch (opt->disc_str[0]) {
	case '4':
		if (!opt->do_adapt) {
			for (it = cfg->begin(); it != cfg->end(); it++) {
				if (it->dynmem && !it->dynmem->adp_addr)
					it->dynmem->adp_addr = it->dynmem->code_addr;
			}
		}
		if (strlen(opt->disc_str) == 1) {
			for (it = cfg->begin(); it != cfg->end(); it++) {
				if (it->dynmem && it->dynmem->adp_addr) {
					found = 1;
					break;
				}
			}
			if (!found)
				goto err;

			disc_str += opt->disc_str;
			disc_str += ";0x0;0x0;";
			disc_str += to_string(it->dynmem->mem_size);
			for (i = 0; i < 3; i++) {
				disc_str += ";";
				disc_str += to_string(it->dynmem->adp_addr);
			}
			opt->disc_str = new char[disc_str.size() + 1];
			opt->disc_str[disc_str.size()] = '\0';
			memcpy(opt->disc_str, disc_str.c_str(),
				disc_str.size());
			cout << "disc_str: " << opt->disc_str << endl;
		} else {
			cerr << "Sorry, not supported, yet!" << endl;
			goto err;
		}
		break;
	default:
		goto err;
	}

	return 0;
err:
	cerr << "Error while preparing discovery!" << endl;
	return -1;
}

static i32 parse_adapt_result (list<CfgEntry> *cfg, char *buf,
			       ssize_t buf_len)
{
	char *part_end = buf;
	ssize_t part_size, ppos = 0;
	u32 i, num_obj = 0;
	string *obj_name = NULL;
	void *code_addr = NULL;
	list<CfgEntry>::iterator it;
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
			if (it->dynmem && !it->dynmem->adp_addr &&
			    it->dynmem->name == *obj_name) {
				it->dynmem->adp_addr = code_addr;
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

static i32 adapt_config (list<CfgEntry> *cfg, char *adp_script)
{
	char pbuf[PIPE_BUF] = { 0 };
	ssize_t read_bytes;
	const char *cmd = (const char *) adp_script;
	char *cmdv[] = {
		adp_script,
		NULL
	};

	if (getuid() == 0)
		goto err;

	read_bytes = run_cmd_pipe(cmd, cmdv, pbuf, sizeof(pbuf));
	if (read_bytes <= 0)
		goto err;
	cout << "Adaption return:" << endl;
	if (pbuf[read_bytes - 1] == '\n')
		cout << pbuf;
	else
		cout << pbuf << endl;

	if (parse_adapt_result(cfg, pbuf, read_bytes) != 0)
		goto err;

	return 0;
err:
	cerr << "Error while running adaption script!" << endl;
	return -1;
}

// mf() callback for read_dynmem_buf()
void process_disc_output (list<CfgEntry> *cfg,
			  void *mem_addr,
			  ssize_t mem_size,
			  void *code_addr,
			  void *stack_offs)
{
	list<CfgEntry>::iterator it;

	cout << "Discovery output: " << endl;
	cout << "m" << hex << mem_addr << dec << ";s"
	     << mem_size << hex << ";c" << code_addr
	     << ";o" << stack_offs << dec << endl;

	// find object and set adp_stack
	for (it = cfg->begin(); it != cfg->end(); it++) {
		if (it->dynmem &&
		    it->dynmem->adp_addr == code_addr) {
			it->dynmem->adp_stack = stack_offs;
			break;
		}
	}
}

// mf() callback for read_dynmem_buf()
void set_dynmem_addr (list<CfgEntry> *cfg,
		      void *mem_addr,
		      ssize_t mem_size,
		      void *code_addr,
		      void *stack_offs)
{
	list<CfgEntry>::iterator it;

	//cout << "m" << hex << mem_addr << ";c"
	//	<< code_addr << dec << endl;

	// find object and set mem_addr
	for (it = cfg->begin(); it != cfg->end(); it++) {
		if (it->dynmem &&
		    it->dynmem->code_addr == code_addr) {
			it->dynmem->mem_addr = mem_addr;
			cout << "Obj. " << it->dynmem->name << "; c"
				<< it->dynmem->code_addr << "; s"
				<< it->dynmem->mem_size << "; created at "
				<< it->dynmem->mem_addr << endl;
			break;
		}
	}
}

// ff() callback for read_dynmem_buf()
void unset_dynmem_addr (list<CfgEntry> *cfg, void *mem_addr)
{
	list<CfgEntry>::iterator it;

	//cout << "f" << hex << mem_addr << dec << endl;
	for (it = cfg->begin(); it != cfg->end(); it++) {
		if (it->dynmem &&
		    it->dynmem->mem_addr == mem_addr) {
			it->dynmem->mem_addr = NULL;
			cout << "Obj. " << it->dynmem->name << "; c"
				<< it->dynmem->code_addr << "; s"
				<< it->dynmem->mem_size << "; freed from "
				<< mem_addr << endl;
			break;
		}
	}
}

void read_dynmem_buf (list<CfgEntry> *cfg, i32 ifd, u8 *pmask,
		      void (*mf)(list<CfgEntry> *, void *,
				 ssize_t, void *, void *),
		      void (*ff)(list<CfgEntry> *, void *))
{
	void *mem_addr = NULL, *code_addr = NULL, *stack_offs = NULL;
	static ssize_t ipos = 0, ilen = 0;
	ssize_t mem_size = 0, tmp_ilen, ppos = 0;
	char *msg_end, *sep_pos;
	char ibuf[4096] = { 0 };
	char scan_ch;

	// read from FIFO and concat. incomplete msgs
	tmp_ilen = read(ifd, ibuf + ipos, sizeof(ibuf) - 1 - ipos);  // always '\0' at end
	if (tmp_ilen > 0) {
		//cout << "ibuf: " << string(ibuf) << endl;
		ilen += tmp_ilen;
		//cout << "ilen: " << ilen << " tmp_ilen " << tmp_ilen << endl;
		ipos += tmp_ilen;

		// parse messages
next:
		msg_end = strchr(ibuf, '\n');
		if (msg_end == NULL)
			return;
		if (sscanf(ibuf, "%c", &scan_ch) != 1)
			goto parse_err;
		switch (scan_ch) {
		case 'm':
			ppos = 1;
			if (sscanf(ibuf + ppos, "%p", &mem_addr) != 1)
				goto parse_err;

			if (!pmask[1])
				goto skip_s;
			sep_pos = strchr(ibuf + ppos, ';');
			if (!sep_pos)
				goto parse_err;
			ppos = sep_pos + 1 - ibuf;
			if (sscanf(ibuf + ppos, "%c", &scan_ch) != 1 ||
			    scan_ch != 's')
				goto parse_err;
			ppos++;
			if (sscanf(ibuf + ppos, "%zd", &mem_size) != 1)
				goto parse_err;
skip_s:
			if (!pmask[2])
				goto skip_c;
			sep_pos = strchr(ibuf + ppos, ';');
			if (!sep_pos)
				goto parse_err;
			ppos = sep_pos + 1 - ibuf;
			if (sscanf(ibuf + ppos, "%c", &scan_ch) != 1 ||
			    scan_ch != 'c')
				goto parse_err;
			ppos++;
			if (sscanf(ibuf + ppos, "%p", &code_addr) != 1)
				goto parse_err;
skip_c:
			if (!pmask[3])
				goto skip_o;
			sep_pos = strchr(ibuf + ppos, ';');
			if (!sep_pos)
				goto parse_err;
			ppos = sep_pos + 1 - ibuf;
			if (sscanf(ibuf + ppos, "%c", &scan_ch) != 1 ||
			    scan_ch != 'o')
				goto parse_err;
			ppos++;
			if (sscanf(ibuf + ppos, "%p", &stack_offs) != 1)
				goto parse_err;
skip_o:
			// call post parsing function
			mf(cfg, mem_addr, mem_size, code_addr, stack_offs);
			break;
		case 'f':
			if (!ff)
				break;
			ppos = 1;
			if (sscanf(ibuf + ppos, "%p", &mem_addr) != 1)
				goto parse_err;

			// call post parsing function
			ff(cfg, mem_addr);
			break;
		}
		// prepare for next msg parsing

		tmp_ilen = msg_end + 1 - ibuf;
		// move rest to the front
		memmove(ibuf, ibuf + tmp_ilen, ilen - tmp_ilen);
		// zero what's behind the rest
		memset(ibuf + ilen - tmp_ilen, 0, tmp_ilen);
		ilen -= tmp_ilen;
		ipos -= tmp_ilen;
		//cout << "ilen: " << ilen << " ipos " << ipos << endl;

		goto next;
	}
	return;

parse_err:
	cerr << "parse error at ppos: " << ppos << endl;
	memset(ibuf, 0, sizeof(ibuf));
	ilen = 0;
	ipos = 0;
	return;
}

i32 main (i32 argc, char **argv, char **env)
{
	char *proc_name = NULL;
	char *adp_script = NULL;
	list<CfgEntry> cfg;
	list<CfgEntry*> *cfg_act;
	list<CfgEntry>::iterator cfg_it;
	list<CfgEntry*>::iterator it;
	list<CfgEntry*> *cfgp_map[256] = { NULL };
	CfgEntry *cfg_en;
	void *mem_addr, *mem_offs = NULL;
	pid_t pid;
	char *home;
	char def_home[] = "~";
	u8 buf[sizeof(i64)] = { 0 };
	u8 pmask[] = {1, 0, 1, 0};
	char ch;
	double tmp_dval;
	i32 ifd = -1, ofd = -1;
	struct app_options opt;

	atexit(restore_getch);

	if (argc < 2)
		usage();

	parse_options(argc, argv, &opt);

	home = getenv(HOME_VAR);
	if (!home)
		home = def_home;

	cfg_act = read_config(argv[optind - 1], home, &proc_name,
			      &adp_script, &cfg, cfgp_map);
	cout << "Found config for \"" << proc_name << "\"." << endl;

	cout << "Config:" << endl;
	output_config(&cfg);
	cout << "Activated:" << endl;
	output_configp(cfg_act);
	if (cfg.empty())
		return -1;

	if (opt.do_adapt && adapt_config(&cfg, adp_script) != 0) {
		cerr << "Error while code address adaption!" << endl;
		return -1;
	}

	if (prepare_discovery(&opt, &cfg) != 0)
		return -1;

	if (prepare_dynmem(&opt, proc_name, &cfg, &ifd, &ofd) != 0) {
		cerr << "Error while dyn. mem. preparation!" << endl;
		return -1;
	}

	pid = proc_to_pid(proc_name);
	if (pid < 0)
		return -1;
	cout << "PID: " << pid << endl;

	if (opt.disc_str) {
		pmask[0] = pmask[1] = pmask[2] = pmask[3] = 1;
		while (1) {
			sleep(1);
			read_dynmem_buf(&cfg, ifd, pmask, process_disc_output,
					NULL);
			if (memattach_test(pid) != 0) {
				cerr << "PTRACE ERROR PID[" << pid << "]!"
				     << endl;
				break;
			}
		}
		for (cfg_it = cfg.begin(); cfg_it != cfg.end(); cfg_it++) {
			if (cfg_it->dynmem && !(cfg_it->dynmem->adp_addr &&
			    cfg_it->dynmem->adp_stack)) {
				cerr << "Discovery failed!" << endl;
				return -1;
			}
		}
		cout << "Discovery successful!" << endl;
		return 0;
	}

	if (prepare_getch_nb() != 0) {
		cerr << "Error while terminal preparation!" << endl;
		return -1;
	}

	if (memattach_test(pid) != 0) {
		cerr << "PTRACE ERROR PID[" << pid << "]!" << endl;
		return -1;
	}

	while (1) {
		sleep(1);
		ch = do_getch();
		if (ch > 0 && cfgp_map[(i32)ch])
			toggle_cfg(cfgp_map[(i32)ch], cfg_act);

		read_dynmem_buf(&cfg, ifd, pmask, set_dynmem_addr,
				unset_dynmem_addr);

		if (memattach(pid) != 0) {
			cerr << "PTRACE ATTACH ERROR PID[" << pid << "]!" << endl;
			return -1;
		}

		for (it = cfg_act->begin(); it != cfg_act->end(); it++) {
			cfg_en = *it;
			if (cfg_en->dynmem) {
				if (!cfg_en->dynmem->mem_addr)
					continue;
				else
					mem_offs = cfg_en->dynmem->mem_addr;
			}

			mem_addr = PTR_ADD(void *, mem_offs, cfg_en->addr);
			if (memread(pid, mem_addr, buf, sizeof(i64)) != 0) {
				cerr << "PTRACE READ MEMORY ERROR PID[" << pid << "]!" << endl;
				return -1;
			}
			change_memory(pid, cfg_en, buf, mem_offs);
		}

		if (memdetach(pid) != 0) {
			cerr << "PTRACE DETACH ERROR PID[" << pid << "]!" << endl;
			return -1;
		}

		for (it = cfg_act->begin(); it != cfg_act->end(); it++) {
			cfg_en = *it;
			if (cfg_en->dynmem) {
				if (!cfg_en->dynmem->mem_addr)
					continue;
				else
					mem_offs = cfg_en->dynmem->mem_addr;
			} else {
				mem_offs = 0;
			}
			cout << cfg_en->name << " at " << hex
				<< PTR_ADD(void *, cfg_en->addr, mem_offs)
				<< ", Data: 0x" << (i64) cfg_en->old_val << dec;
			if (cfg_en->is_float) {
				memcpy(&tmp_dval, &cfg_en->old_val, sizeof(i64));
				cout << " (" << tmp_dval << ")" << endl;
			} else {
				cout << " (" << cfg_en->old_val << ")" << endl;
			}
		}

	}

	return 0;
}
