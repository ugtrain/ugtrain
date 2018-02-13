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
#include <lib/system.h>


static inline void
init_atexit (void (*function)(void))
{
	if (likely(atexit(function) == 0))
		return;
	cerr << "Error while registering exit handler!" << endl;
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
init_dmparams (struct dynmem_params *dmparams, struct app_options *opt,
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

static inline void
test_memattach (pid_t pid)
{
	if (likely(memattach_test(pid) == 0))
		return;
	cerr << "MEMORY ATTACHING TEST ERROR PID[" << pid << "]!" << endl;
	exit(-1);
}

static inline void
memattach_err_once (pid_t pid)
{
	static bool reported = false;

	if (!reported) {
		cerr << "MEMORY ATTACH ERROR PID[" << pid << "]!" << endl;
		reported = true;
	}
}

static inline void
detachmem (pid_t pid)
{
	if (likely(memdetach(pid) == 0))
		return;
	cerr << "MEMORY DETACH ERROR PID[" << pid << "]!" << endl;
	exit(-1);
}

static inline i32
read_memory (pid_t pid, ptr_t mem_addr, value_t *buf, const char *pfx)
{
	i32 ret;

	ret = memread(pid, mem_addr, buf, sizeof(value_t));
	if (ret)
		cerr << pfx << " READ ERROR PID[" << pid << "] ("
		     << hex << mem_addr << dec << ")!" << endl;
	return ret;
}

static inline i32
write_memory (pid_t pid, ptr_t mem_addr, value_t *buf, const char *pfx)
{
	i32 ret;

	ret = memwrite(pid, mem_addr, buf, sizeof(value_t));
	if (ret)
		cerr << pfx << " WRITE ERROR PID[" << pid << "] ("
		     << hex << mem_addr << dec << ")!" << endl;
	return ret;
}

#endif
