/* preload_l.c:    preloader functions to hook a lib into the game
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

#ifdef __linux__

#include <stdlib.h>
#include <string.h>

/* local includes */
#include "preload.h"


/*
 * This function has been taken from 'glc' by nullkey aka
 * Pyry Haulos <pyry.haulos@gmail.com> and improved
 */
static i32 env_append (const char *name, const char *val, char separator)
{
	size_t env_len;
	const char *old_env;
	char *new_env;

	old_env = getenv(name);

	if (old_env != NULL) {
		env_len = strlen(old_env) + strlen(val) + 2;
		new_env = malloc(env_len);
		if (!new_env)
			goto err;

		memcpy(new_env, old_env, strlen(old_env));
		memcpy(&new_env[strlen(old_env) + 1], val, strlen(val));

		new_env[strlen(old_env)] = separator;
		new_env[env_len - 1] = '\0';
	} else {
		env_len = strlen(val) + 1;
		new_env = malloc(env_len);
		if (!new_env)
			goto err;

		memcpy(new_env, val, env_len - 1);
		new_env[env_len - 1] = '\0';
	}

	setenv(name, new_env, 1);
	free(new_env);

	return 0;
err:
	return -1;
}

i32 preload_library (char *lib_path)
{
	i32 ret = 0;

	if (!lib_path)
		goto out;

	if (lib_path[0] != '-')
		ret = env_append(PRELOAD_VAR, lib_path, ':');
out:
	return ret;
}

#endif
