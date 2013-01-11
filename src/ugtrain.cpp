/* ugtrain.cpp:    freeze values in process memory (game trainer)
 *
 * Copyright (c) 2012, by:      Sebastian Riemer
 *    All rights reserved.      Ernst-Reinke-Str. 23
 *                              10369 Berlin, Germany
 *                             <sebastian.riemer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <libgcheater.h>
#include <sys/wait.h>
#include <limits.h>

// local includes
#include "getch.h"
#include "cfgentry.h"
#include "parser.h"
using namespace std;

#define PROG_NAME  "ugtrain"
#define DYNMEM_IN  "/tmp/memhack_out"
#define DYNMEM_OUT "/tmp/memhack_in"


void output_configp (list<CfgEntry*> *cfg)
{
	if (!cfg || cfg->empty()) {
		cout << "<none>" << endl;
		return;
	}

	CfgEntry *cfg_en;

	list<CfgEntry*>::iterator it;
	for (it = cfg->begin(); it != cfg->end(); it++) {
		cfg_en = *it;
		cout << cfg_en->name << " " << hex << cfg_en->addr << dec;
		cout << " " << cfg_en->size << " " << cfg_en->value << endl;
	}
}

static void output_checks (CfgEntry *cfg_en)
{
	list<CheckEntry> *chk_lp;

	if (cfg_en->check > DO_UNCHECKED) {
		cout << "check " << hex << cfg_en->addr << dec;
		if (cfg_en->check == DO_LT)
			cout << " <";
		else if (cfg_en->check == DO_GT)
			cout << " >";
	}
	cout << " " << cfg_en->value << endl;

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
		cout << " " << it->value << endl;
	}
}

static void output_config (list<CfgEntry> *cfg)
{
	if (!cfg || cfg->empty()) {
		cout << "<none>" << endl;
		return;
	}

	CfgEntry cfg_en;

	list<CfgEntry>::iterator it;
	for (it = cfg->begin(); it != cfg->end(); it++) {
		cfg_en = *it;
		if (cfg_en.dynmem)
			cout << "dynmem: " << cfg_en.dynmem->mem_size << " "
			<< hex << cfg_en.dynmem->code_addr << " "
			<< cfg_en.dynmem->stack_offs << dec << endl;
		cout << cfg_en.name << " " << hex << cfg_en.addr << dec;
		cout << " " << cfg_en.size << " " << cfg_en.value << endl;
		output_checks(&cfg_en);
	}
}

static pid_t proc_to_pid (string *proc_name)
{
	pid_t pid;
	i32 status, fds[2], bytes_read;
	char pbuf[PIPE_BUF] = {0};
	string shell_cmd("pidof -s ");

	shell_cmd.append(*proc_name);

	if (pipe(fds) < 0) {
		perror("pipe");
		return -1;
	}
	pid = fork();
	if (pid < 0) {
		perror("fork");
		close(fds[1]);
		close(fds[0]);
		return -1;
	} else if (pid == 0) {
		if (dup2(fds[1], 1) < 0) {
			perror("dup2");
			close(fds[1]);
			close(fds[0]);
			exit(-1);
		}
		close(fds[0]);
		if (execlp("sh", "sh", "-c", shell_cmd.c_str(), NULL) < 0) {
			perror("execlp");
			close(fds[1]);
			exit(-1);
		}
	} else {
		close(fds[1]);
		waitpid(pid, &status, 0);
		bytes_read = read(fds[0], pbuf, PIPE_BUF);
		if (bytes_read < 0) {
			perror("pipe read");
			close(fds[0]);
			return -1;
		} else if (bytes_read <= 1) {
			cerr << "Unable to find PID for \"" << *proc_name << "\"!" << endl;
			close(fds[0]);
			return -1;
		}
		close(fds[0]);
	}

	cout << "Pipe: " << pbuf;

	if (isdigit(pbuf[0])) {
		pid = atoi(pbuf);
		if (pid > 1)
			return pid;
	}

	cerr << "Found PID is invalid!" << endl;
	return -1;
}

void usage()
{
	cerr << "Not enough arguments!" << endl;
	cerr << "Call: " << PROG_NAME << " <config>" << endl;
	exit(-1);
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
			return check_mem_val((long) chk_en.value, chk_buf, chk_en.check);
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
			return check_mem_val((unsigned long) chk_en.value, chk_buf, chk_en.check);
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
	u8 chk_buf[10];
	void *mem_addr;

	cfg_en->old_val = *(T *)buf;

	if (cfg_en->checks) {
		chk_lp = cfg_en->checks;
		list<CheckEntry>::iterator it;
		for (it = chk_lp->begin(); it != chk_lp->end(); it++) {
			mem_addr = (void *) ((ptr_t)mem_offs + (ptr_t)it->addr);

			if (gc_get_memory(pid, mem_addr, chk_buf, sizeof(long)) != 0) {
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
		mem_addr = (void *) ((ptr_t)mem_offs + (ptr_t)cfg_en->addr);

		if (gc_set_memory(pid, mem_addr, buf, sizeof(long)) != 0) {
			cerr << "PTRACE WRITE MEMORY ERROR PID[" << pid << "]!" << endl;
			exit(-1);
		}
	}
}

static void change_memory (pid_t pid, CfgEntry *cfg_en, u8 *buf, void *mem_offs)
{
	if (cfg_en->is_signed) {
		switch (cfg_en->size) {
		case 64:
			change_mem_val(pid, cfg_en, (long) cfg_en->value, buf, mem_offs);
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
			change_mem_val(pid, cfg_en, (unsigned long) cfg_en->value, buf, mem_offs);
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

i32 main (i32 argc, char **argv)
{
	string proc_name;
	list<CfgEntry> cfg;
	list<CfgEntry*> *cfg_act;
	list<CfgEntry*>::iterator it;
	list<CfgEntry*> *cfgp_map[256] = { NULL };
	CfgEntry *cfg_en;
	void *mem_addr, *mem_offs = NULL;
	pid_t pid;
	u8 buf[10] = { 0 };
	char ch;

	atexit(restore_getch);

	if (argc < 2)
		usage();

	cfg_act = read_config(argv[1], &proc_name, &cfg, cfgp_map);
	cout << "Found config for \"" << proc_name << "\"." << endl;

	cout << "Config:" << endl;
	output_config(&cfg);
	cout << "Activated:" << endl;
	output_configp(cfg_act);
	if (cfg.empty())
		return -1;

	pid = proc_to_pid(&proc_name);
	if (pid < 0)
		return -1;
	cout << "PID: " << pid << endl;

	if (prepare_getch_nb() != 0) {
		cerr << "Error while terminal preparation!" << endl;
		return -1;
	}

	if (gc_ptrace_test(pid) != 0) {
		cerr << "PTRACE ERROR PID[" << pid << "]!" << endl;
		return -1;
	}

	while (1) {
		sleep(1);
		ch = do_getch();
		if (ch > 0 && cfgp_map[(i32)ch])
			toggle_cfg(cfgp_map[(i32)ch], cfg_act);

		if (gc_ptrace_stop(pid) != 0) {
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

			mem_addr = (void *) ((ptr_t)mem_offs + (ptr_t)cfg_en->addr);
			if (gc_get_memory(pid, mem_addr, buf, sizeof(long)) != 0) {
				cerr << "PTRACE READ MEMORY ERROR PID[" << pid << "]!" << endl;
				return -1;
			}
			change_memory(pid, cfg_en, buf, mem_offs);
		}

		if (gc_ptrace_continue(pid) != 0) {
			cerr << "PTRACE DETACH ERROR PID[" << pid << "]!" << endl;
			return -1;
		}

		for (it = cfg_act->begin(); it != cfg_act->end(); it++) {
			cfg_en = *it;
			cout << "Addr: " << hex << cfg_en->addr << " Data: 0x"
			     << (long) cfg_en->old_val << endl << dec;
		}

	}

	return 0;
}
