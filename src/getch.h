/* getch.h:    multi-platform getch() implementation
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

#ifndef GETCH_H
#define GETCH_H

#ifdef __cplusplus
extern "C" {
#endif
	int  prepare_getch (void);
	int  prepare_getch_nb (void);
	char do_getch      (void);
	void restore_getch (void);
#ifdef __cplusplus
};
#endif

#endif
