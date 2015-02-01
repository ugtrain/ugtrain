/* discovery.h:    discover dynamic memory objects
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

#ifndef DISCOVERY_H
#define DISCOVERY_H

// local includes
#include <lib/maps.h>
#include <lib/system.h>
#include <cfgentry.h>
#include <common.h>
#include <options.h>

#define DISC_EXIT	0
#define DISC_NEXT	1
#define DISC_OKAY	2

// parameter for run_stage1234_loop()
struct disc_loop_pp {
	i32 ifd;
	struct app_options *opt;
};

i32  prepare_discovery  (struct app_options *opt, list<CfgEntry> *cfg);
void run_stage1234_loop (void *argp);
void run_stage5_loop    (list<CfgEntry> *cfg, i32 ifd, i32 dfd, i32 pmask,
			 pid_t pid, ptr_t code_offs);
i32  postproc_discovery (struct app_options *opt, list<CfgEntry> *cfg,
			 list<struct region> *rlist, vector<string> *lines);

/* Some Linux distributions use the GCC options -pie and -fPIE
   by default for hardened security. Together with ASLR, the binary
   is loaded each execution to another memory location/address. We
   have to find out that memory address and use it as an offset. This
   applies to code addresses on the stack and static memory as well.
   PIE is detected from that load address. */
static inline ptr_t handle_exe_region (struct region *r, i32 ofd,
				       struct app_options *opt)
{
	/* PIE detection */
	ptr_t exe_offs = get_exe_offs(r->start);

	opt->code_offs = exe_offs;
	cout << "exe_offs: 0x" << hex << exe_offs << dec << endl;
	if (exe_offs)
		cout << "PIE (position independent executable) detected!"
		     << endl;
	return exe_offs;
}

static inline void handle_pie (struct app_options *opt, list<CfgEntry> *cfg,
	i32 ifd, i32 ofd, pid_t pid, list<struct region> *rlist)
{
	struct pmap_params params;
	char buf[PIPE_BUF];
	ssize_t rbytes;
	list<CfgEntry>::iterator cfg_it;
	list<struct region>::iterator it;
	DynMemEntry *old_dynmem = NULL;
	ptr_t exe_offs = 0;
	ssize_t wbytes;
	char exe_path[MAPS_MAX_PATH];
	char obuf[PIPE_BUF];
	i32 osize = 0;

	if (!opt->pure_statmem) {
		while (true) {
			sleep_sec_unless_input(1, ifd);
			rbytes = read(ifd, buf, sizeof(buf));
			if (rbytes == 0 ||
			    (rbytes < 0 && errno == EAGAIN))
				continue;
			else
				break;
		}
	}
	get_exe_path_by_pid(pid, exe_path, sizeof(exe_path));
	params.exe_path = exe_path;
	params.rlist = rlist;
	read_regions(pid, &params);
	list_for_each (rlist, it) {
		if (it->type == REGION_TYPE_EXE && it->flags.exec) {
			exe_offs = handle_exe_region(&(*it), ofd, opt);
			break;
		}
	}
	if (opt->pure_statmem || opt->disc_str)
		return;
	list_for_each (cfg, cfg_it) {
		if (!cfg_it->dynmem || cfg_it->dynmem == old_dynmem)
			continue;
		old_dynmem = cfg_it->dynmem;
		osize += snprintf(obuf + osize, sizeof(obuf) - osize,
			PRI_PTR ";", exe_offs);
	}
	// Write code offsets to output FIFO
	wbytes = write(ofd, obuf, osize);
	if (wbytes < osize)
		perror("FIFO write");
}

#endif
