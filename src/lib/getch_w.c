/* getch_w.c:    getch() for Windows with system functions
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
 */

#if defined(__WINNT__) || defined (__WIN32__)

/* local includes */
#include "getch.h"

int prepare_getch (void)
{
	return 0;
}

int prepare_getch_nb (void)
{
	int rc;

	rc = prepare_getch();
	return rc;
}

char do_getch (void)
{
	char ch = '\0';
	return ch;
}

void set_getch_nb (int nb)
{
}

void restore_getch (void)
{
}

#endif
