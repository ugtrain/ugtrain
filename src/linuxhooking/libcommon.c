/* libcommon.c:    common functions for preloaded libs
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcommon.h"

#define PFX "[libcommon] "


void rm_from_env (char *env_name, char *pattern, char separator)
{
	char *start, *end, *found, *env_var = getenv(env_name);

	if (!env_var)
		goto out;
	pr_dbg("old: %s=%s\n", env_name, env_var);

	for (start = env_var;;) {
		end = strchr(start, separator);
		if (end)
			*end = '\0';
		found = strstr(start, pattern);
		if (found) {
			if (end) {
				end++;
				memmove(start, end, strlen(end) + 1);
			} else {
				memset(start, 0, strlen(start));
			}
			break;
		} else if (end) {
			*end = separator;
			start = ++end;
		} else {
			goto out;
		}
	}
	pr_dbg("new: %s=%s\n", env_name, env_var);
	setenv(env_name, env_var, 1);
out:
	return;
}
