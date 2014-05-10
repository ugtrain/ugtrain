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

#define DISC_EXIT	0
#define DISC_NEXT	1
#define DISC_OKAY	2

i32  prepare_discovery  (struct app_options *opt, list<CfgEntry> *cfg);
void run_stage1234_loop (void *argp);
void run_stage5_loop    (list<CfgEntry> *cfg, i32 ifd, i32 pmask, pid_t pid);
i32  postproc_discovery (struct app_options *opt, list<CfgEntry> *cfg,
			 vector<string> *lines);

#endif
