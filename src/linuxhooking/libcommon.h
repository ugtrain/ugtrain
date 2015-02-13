/* libcommon.h:    common functions for preloaded libs
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

#ifndef LIBCOMMON_H
#define LIBCOMMON_H

#include <lib/preload.h>
#include <common.h>

#define DEBUG 0
#define DEBUG_MEM 0    /* much output */

#define USE_DEBUG_LOG 0
#define DBG_FILE_VAR  dfile

#if !DEBUG && !USE_DEBUG_LOG
	#define printf(...) do { } while (0);
#endif

#define pr_fmt(fmt) PFX fmt
#if USE_DEBUG_LOG

#define USE_BOTH_DBG 0

static FILE *dfile = NULL;

#define pr_dbg_file(fmt, ...) \
	if (DBG_FILE_VAR) { \
		fprintf(DBG_FILE_VAR, pr_fmt(fmt), ##__VA_ARGS__); \
		fflush(DBG_FILE_VAR); \
	}

#if USE_BOTH_DBG
#if DEBUG
#define printf(...) \
	pr_dbg_file(fmt, ##__VA_ARGS__); \
	fprintf(stdout, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dbg(fmt, ...) \
	pr_dbg_file(fmt, ##__VA_ARGS__); \
	fprintf(stdout, pr_fmt(fmt), ##__VA_ARGS__)
#else
#define printf(...) do { } while (0);
#define pr_dbg(...) do { } while (0);
#endif
#define pr_out(fmt, ...) \
	pr_dbg_file(fmt, ##__VA_ARGS__); \
	fprintf(stdout, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...) \
	pr_dbg_file(fmt, ##__VA_ARGS__); \
	fprintf(stderr, pr_fmt(fmt), ##__VA_ARGS__)
#else  /* !USE_BOTH_DBG */
#if DEBUG
#define printf(...) \
	pr_dbg_file(fmt, ##__VA_ARGS__)
#define pr_dbg(fmt, ...) \
	pr_dbg_file(fmt, ##__VA_ARGS__)
#else
#define printf(...) do { } while (0);
#define pr_dbg(...) do { } while (0);
#endif
#define pr_out(fmt, ...) \
	pr_dbg_file(fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) \
	pr_dbg_file(fmt, ##__VA_ARGS__)
#endif  /* USE_BOTH_DBG */

#else  /* !USE_DEBUG_LOG */

#define pr_dbg(fmt, ...) \
	printf(pr_fmt(fmt), ##__VA_ARGS__)
#define pr_out(fmt, ...) \
	fprintf(stdout, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...) \
	fprintf(stderr, pr_fmt(fmt), ##__VA_ARGS__)

#endif  /* USE_DEBUG_LOG */



void rm_from_env (char *env_name, char *pattern, char separator);

#endif
