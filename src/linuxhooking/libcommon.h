/* libcommon.h:    common functions for preloaded libs
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

#ifndef LIBCOMMON_H
#define LIBCOMMON_H

#include "../common.h"

#define DEBUG 0
#if !DEBUG
	#define printf(...) do { } while (0);
#endif
#define DEBUG_MEM 0    /* much output */

#define pr_fmt(fmt) PFX fmt
#define pr_dbg(fmt, ...) \
	printf(pr_fmt(fmt), ##__VA_ARGS__)
#define pr_out(fmt, ...) \
	fprintf(stdout, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...) \
	fprintf(stderr, pr_fmt(fmt), ##__VA_ARGS__)


void rm_from_env (char *env_name, char *pattern, char separator);

#endif
