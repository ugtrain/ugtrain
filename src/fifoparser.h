/* fifoparser.h:    parsing functions to read from FIFO to cfg
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

#ifndef FIFOPARSER_H
#define FIFOPARSER_H

#include <list>
#include "common.h"
#include "cfgentry.h"
#include "system.h"

#define PARSE_M 1
#define PARSE_S 2
#define PARSE_C 4
#define PARSE_O 8

struct post_parse {
	char	*ibuf;
	ssize_t	ppos;
	void	*argp;
};

#define MF_PARAMS \
	list<CfgEntry> *cfg, struct post_parse *pp, ptr_t heap_start, \
	ptr_t mem_addr, size_t mem_size, ptr_t code_offs, ptr_t code_addr, \
	ptr_t stack_offs

#define FF_PARAMS \
	list<CfgEntry> *cfg, void *argp, ptr_t mem_addr

ssize_t read_dynmem_buf (list<CfgEntry> *cfg, void *argp, i32 ifd, i32 pmask,
			 bool reverse, ptr_t code_offs, void (*mf)(MF_PARAMS),
			 void (*ff)(FF_PARAMS));
#endif
