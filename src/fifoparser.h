/* fifoparser.h:    parsing functions to read from FIFO to cfg
 *
 * Copyright (c) 2012..2018 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef FIFOPARSER_H
#define FIFOPARSER_H

#include <list>

// local includes
#include <lib/system.h>
#include <cfgentry.h>
#include <common.h>
#include <options.h>

#define PARSE_S (1 << 0)
#define PARSE_C (1 << 1)
#define PARSE_O (1 << 2)

// late PIC discovery handling parameters
struct lf_disc_params {
	string *disc_lib;
	pid_t  pid;
	i32    ofd;
};

// malloc queue to delay malloc processing
struct mqueue {
	char    *data;
	ssize_t size;
	ssize_t end;
};

// late PIC handling parameters
struct lf_params {
	list<CfgEntry>   *cfg;
	struct list_head *rlist;
	pid_t pid;
	i32   ofd;
};

// stack end handling parameters
struct sf_params {
	Options *opt;
};

// parameter for the FIFO parser
struct dynmem_params {
	// internal, avoid using
	struct mqueue    _mqueue;
	struct lf_params _lfparams;
	struct sf_params _sfparams;

	struct mqueue    *mqueue;
	struct lf_params *lfparams;
	struct sf_params *sfparams;
};

struct post_parse {
	char	*ibuf;
	ssize_t	ppos;
	char    *msg_end;
	void	*argp;
};

#define MF_PARAMS \
	list<CfgEntry> *cfg, struct post_parse *pp, \
	ptr_t mem_addr, size_t mem_size, ptr_t code_offs, ptr_t code_addr, \
	ptr_t stack_offs

#define FF_PARAMS \
	list<CfgEntry> *cfg, void *argp, ptr_t mem_addr

#define LF_PARAMS \
	void *argp, string *lib_name

#define SF_PARAMS \
	list<CfgEntry> *cfg, void *argp, ptr_t stack_end

/* parsing call back functions */
struct parse_cb {
	void (*mf)(MF_PARAMS);
	void (*ff)(FF_PARAMS);
	void (*lf)(LF_PARAMS);
	void (*sf)(SF_PARAMS);
};

ssize_t read_dynmem_buf (list<CfgEntry> *cfg, void *argp, i32 ifd, i32 pmask,
			 bool reverse, ptr_t code_offs, struct parse_cb *pcb);

ssize_t parse_dynmem_buf (list<CfgEntry> *cfg, void *argp, char *ibuf,
			  ssize_t *ilen, ssize_t tmp_ilen, i32 pmask,
			  bool reverse, ptr_t code_offs, struct parse_cb *pcb);
#endif
