/* getch_l.c:    getch() for Linux with terminal functions
 *
 * Copyright (c) 2012, by:      Sebastian Riemer
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

#ifdef __linux__

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
/* local includes */
#include "getch.h"

static struct termios saved_tty, raw_tty;
static int tty_changed = 0;

int prepare_getch (void)
{
	struct termios new_tty;

	if (tty_changed)
		return 0;
	if (tcgetattr(STDIN_FILENO, &saved_tty) == -1)
		return -1;
	new_tty = saved_tty;
	new_tty.c_lflag &= ~(ICANON | ECHO);
	new_tty.c_oflag &= ~(TAB3);
	new_tty.c_cc[VMIN] = 1;
	new_tty.c_cc[VTIME] = 0;

	tty_changed = 1;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_tty) == -1)
		return -1;
	tcgetattr(STDIN_FILENO, &raw_tty);

	return 0;
}

int prepare_getch_nb (void)
{
	int rc;

	rc = prepare_getch();
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	return rc;
}

char do_getch (void)
{
	char ch;
	int cnt = 1;

	cnt = read(STDIN_FILENO, &ch, cnt);

	tcflush(STDIN_FILENO, TCIFLUSH);
	if (cnt < 0)
		return -1;
	return ch;
}

void set_getch_nb (int nb)
{
	int flags;

	if (!nb) {
		flags = fcntl(STDIN_FILENO, F_GETFL, 0);
		fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
	} else {
		fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	}
}

void restore_getch (void)
{
	int flags;

	/* disable nonblocking */
	flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);

	if (tty_changed) {
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty);
		tty_changed = 0;
	}
}

#endif
