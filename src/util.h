/* util.h:    C++ utility functions
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

#ifndef UTIL_H
#define UTIL_H

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <cstdlib>
#include <fstream>

// local includes
#include <common.h>
#include <cfgentry.h>
#include <fifoparser.h>
#include <lib/memattach.h>
#include <lib/maps.h>
#include <lib/system.h>

#define CFGP_MAP_SIZE 128


/* Skip error message printing until next value successfully read */
extern bool g_read_error_no_warning;

class Globals {
public:
	Options *opt;
	vector<string> *cfg_lines;
	list<CfgEntry> *cfg;
	list<CfgEntry*> *cfg_act;
	list<CfgEntry*> **cfgp_map;
	struct dynmem_params *dmparams;
	struct list_head *rlist;
};


/* Function declarations */
Globals *get_globals (void);
void clear_config (void);
void cleanup_ugtrain_atexit (void);

/* Inline functions */
static inline void
init_atexit (void (*function)(void))
{
	if (likely(atexit(function) == 0))
		return;
	ugerr << "Error while registering exit handler!" << endl;
	exit(-1);
}

static inline void
init_dmparams_early (struct dynmem_params *dmparams)
{
	struct mqueue *mq;

	mq = dmparams->mqueue = &dmparams->_mqueue;
	dmparams->lfparams = &dmparams->_lfparams;
	dmparams->sfparams = &dmparams->_sfparams;

	mq->size = 4 * PIPE_BUF;
	mq->end = 0;
	mq->data = (char *) malloc(mq->size);
	if (!mq->data)
		exit(-1);
	mq->data[0] = '\0';
	mq->data[mq->size - 1] = '\0';
}

static inline void
init_dmparams (struct dynmem_params *dmparams, Options *opt,
	       list<CfgEntry> *cfg, i32 ofd, pid_t pid, struct list_head *rlist)
{
	dmparams->lfparams->cfg = cfg;
	dmparams->lfparams->pid = pid;
	dmparams->lfparams->ofd = ofd;
	dmparams->lfparams->rlist = rlist;
	dmparams->sfparams->opt = opt;
}

static inline bool
tool_is_available (char *name)
{
	bool ret = false;
	char *tmp_path = get_abs_app_path(name);
	ugout << "Checking if " << name << " is available: ";
	if (!tmp_path) {
		cout << "no" << endl;
		ugerr << "Please consider installing " << name << "!" << endl;
	} else {
		cout << "yes" << endl;
		free(tmp_path);
		ret = true;
	}
	return ret;
}

static inline void
test_memattach (pid_t pid, i32 *fd)
{
	if (likely(memattach_test(pid, fd) == 0))
		return;
	ugerr << "MEMORY ATTACHING TEST ERROR PID[" << pid << "]!" << endl;
	show_errorstr();
	exit(-1);
}

static inline void
memattach_err_once (pid_t pid)
{
	static bool reported = false;

	if (!reported) {
		ugerr << "MEMORY ATTACH ERROR PID[" << pid << "]!" << endl;
		reported = true;
	}
}

static inline void
memdetach_err_once (pid_t pid)
{
	static bool reported = false;

	if (!reported) {
		ugerr << "MEMORY DETACH ERROR PID[" << pid << "]!" << endl;
		reported = true;
	}
}

static inline i32
_read_memory (pid_t pid, ptr_t mem_addr, void *buf, size_t buf_len, const char *pfx)
{
	i32 ret;

	ret = memread(pid, mem_addr, buf, buf_len);
	if (ret && !g_read_error_no_warning)
		ugerr << pfx << " READ ERROR PID[" << pid << "] ("
		      << hex << mem_addr << dec << ")!" << endl;
	return ret;
}

static inline i32
read_memory (pid_t pid, ptr_t mem_addr, value_t *buf, const char *pfx)
{
	return _read_memory(pid, mem_addr, buf, sizeof(value_t), pfx);
}

static inline i32
_write_memory (pid_t pid, ptr_t mem_addr, void *buf, size_t buf_len, const char *pfx)
{
	i32 ret;

	ret = memwrite(pid, mem_addr, buf, buf_len);
	if (ret)
		ugerr << pfx << " WRITE ERROR PID[" << pid << "] ("
		      << hex << mem_addr << dec << ")!" << endl;
	return ret;
}

static inline i32
write_memory (pid_t pid, ptr_t mem_addr, value_t *buf, const char *pfx)
{
	return _write_memory(pid, mem_addr, buf, sizeof(value_t), pfx);
}

#endif
