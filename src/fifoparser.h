/* fifoparser.h:    parsing functions to read from FIFO to cfg
 *
 * Copyright (c) 2013, by:      Sebastian Riemer
 *    All rights reserved.     <sebastian.riemer@gmx.de>
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

#ifndef FIFOPARSER_H
#define FIFOPARSER_H

#include <list>
#include "common.h"
#include "cfgentry.h"

#define PARSE_M 1
#define PARSE_S 2
#define PARSE_C 4
#define PARSE_O 8

struct post_parse {
	char	*ibuf;
	ssize_t	ppos;
	void	*argp;
};

i32 read_dynmem_buf (list<CfgEntry> *cfg, void *argp, i32 ifd, int pmask, u8 reverse,
		     void (*mf)(list<CfgEntry> *, struct post_parse *, void *,
				void *, size_t, void *, void *),
		     void (*ff)(list<CfgEntry> *, void *, void *));
#endif
