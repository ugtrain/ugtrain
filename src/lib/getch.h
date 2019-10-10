/* getch.h:    multi-platform getch() implementation
 *
 * Copyright (c) 2012..2016 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#ifdef __linux__
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
	int  prepare_getch (void);
	void restore_getch (void);
#ifdef __cplusplus
};
#endif

#ifdef __linux__
static inline int prepare_getch_nb (void)
{
	int rc;

	rc = prepare_getch();
	if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
		rc = -1;
	return rc;
}

static inline char do_getch (void)
{
	char ch = 0;
	int cnt = 1;

	cnt = read(STDIN_FILENO, &ch, cnt);

	tcflush(STDIN_FILENO, TCIFLUSH);
	if (cnt < 0)
		return -1;
	return ch;
}

static inline int set_getch_nb (int nb)
{
	int flags;

	if (!nb) {
		flags = fcntl(STDIN_FILENO, F_GETFL, 0);
		if (fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK) != 0)
			goto err;
	} else {
		if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
			goto err;
	}
	return 0;
err:
	return -1;
}

#else

static inline int prepare_getch_nb (void)
{
	int rc;

	rc = prepare_getch();
	return rc;
}

static inline char do_getch (void)
{
	char ch = '\0';
	return ch;
}

static inline int set_getch_nb (int nb)
{
	return 0;
}
#endif
