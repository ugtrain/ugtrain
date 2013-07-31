/* discovery.h:    discover dynamic memory objects
 *
 * Copyright (c) 2013, by:      Sebastian Riemer
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
			 string *cfg_path, vector<string> *lines);

#endif
