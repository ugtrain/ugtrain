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

#include "options.h"
#include "cfgentry.h"
#include "common.h"
#include "maps.h"

#define DISC_EXIT	0
#define DISC_NEXT	1
#define DISC_OKAY	2

// parameter for run_stage1234_loop()
struct disc_loop_pp {
	i32 ifd;
	struct app_options *opt;
};

i32  prepare_discovery  (struct app_options *opt, list<CfgEntry> *cfg);
void prepare_backtrace  (struct app_options *opt, i32 ifd, i32 ofd, pid_t pid,
			 list<struct region> *rlist);
void run_stage1234_loop (void *argp);
void run_stage5_loop    (list<CfgEntry> *cfg, i32 ifd, i32 pmask, pid_t pid,
			 void *code_offs);
i32  postproc_discovery (struct app_options *opt, list<CfgEntry> *cfg,
			 vector<string> *lines);

/* Some Linux distributions use the GCC options -pie and -fPIE
   by default for hardened security. Together with ASLR, the binary
   is loaded each execution to another memory location. We have to
   find out that memory location and use it as an offset. This
   applies to code addresses on the stack and static memory as well.
   PIE is detected from that offset. */
static inline void handle_exe_region (struct region *r, i32 ofd,
				      struct app_options *opt)
{
	ssize_t wbytes;
	char obuf[PIPE_BUF];
	u32 osize;
	void *code_offs;

	cout << "Found exe, start: " << hex << r->start
	     << ", end: " << r->start + r->size << dec << endl;
	/* PIE detection: x86 offs: 0x8048000, x86_64 offs: 0x400000 */
	if (r->start == 0x8048000UL ||
	    (r->start == 0x400000UL))
		code_offs = NULL;
	else
		code_offs = (void *) r->start;
	opt->code_offs = code_offs;
	cout << "code_offs: " << opt->code_offs << endl;
	if (code_offs)
		cout << "PIE (position independent executable) detected!"
		     << endl;
	// Write code offset to output FIFO
	osize = snprintf(obuf, sizeof(obuf), "%p\n", opt->code_offs);
	wbytes = write(ofd, obuf, osize);
	if (wbytes < osize)
		perror("FIFO write");
}

static void handle_pie (struct app_options *opt, i32 ifd, i32 ofd, pid_t pid,
			list<struct region> *rlist)
{
	char buf[PIPE_BUF];
	ssize_t rbytes;
	list<struct region>::iterator it;

	while (true) {
		sleep_sec_unless_input(1, ifd, -1);
		rbytes = read(ifd, buf, sizeof(buf));
		if (rbytes == 0 ||
		    (rbytes < 0 && errno == EAGAIN))
			continue;
		else
			break;
	}
	read_regions(pid, rlist);
	list_for_each (rlist, it) {
		if (it->type == REGION_TYPE_EXE && it->flags.exec) {
			handle_exe_region(&(*it), ofd, opt);
			break;
		}
	}
}

#endif
