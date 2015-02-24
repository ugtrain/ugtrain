/* aslr.h:    handle Address Space Layout Randomization (ASLR)
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
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

#ifndef ASLR_H
#define ASLR_H

// local includes
#include <lib/maps.h>
#include <cfgentry.h>
#include <common.h>
#include <options.h>

void handle_pie (struct app_options *opt, list<CfgEntry> *cfg, i32 ifd,
		 i32 ofd, pid_t pid, list<struct region> *rlist);

void do_disc_pic_work (pid_t pid, struct app_options *opt,
		       i32 ifd, i32 ofd, list<struct region> *rlist);

#endif
