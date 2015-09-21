/* getch_w.c:    getch() for Windows with system functions
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
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

void restore_getch (void)
{
}

#endif
