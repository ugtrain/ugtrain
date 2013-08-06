/* fifoparser.h:    parsing functions to read from FIFO to cfg
 *
 * Copyright (c) 2013, by:      Sebastian Riemer
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

#include <cstring>
#include <stdio.h>
#include <limits.h>
#include "fifoparser.h"

i32 read_dynmem_buf (list<CfgEntry> *cfg, void *argp, i32 ifd, int pmask,
		     void (*mf)(list<CfgEntry> *, struct post_parse *, void *,
				void *, ssize_t, void *, void *),
		     void (*ff)(list<CfgEntry> *, void *, void *))
{
	void *mem_addr = NULL, *code_addr = NULL, *stack_offs = NULL;
	static void *heap_start = NULL;
	static ssize_t ipos = 0, ilen = 0;
	ssize_t mem_size = 0, tmp_ilen, ppos = 0;
	char *msg_end, *sep_pos;
	static char ibuf[PIPE_BUF] = { 0 };
	char scan_ch;
	struct post_parse pp;

	// read from FIFO and concat. incomplete msgs
	tmp_ilen = read(ifd, ibuf + ipos, sizeof(ibuf) - 1 - ipos);  // always '\0' at end
	if (tmp_ilen > 0) {
		//cout << "ibuf: " << string(ibuf) << endl;
		ilen += tmp_ilen;
		//cout << "ilen: " << ilen << " tmp_ilen " << tmp_ilen << endl;
		//cout << "ipos: " << ipos << endl;
		ipos += tmp_ilen;

		// parse messages
next:
		msg_end = strchr(ibuf, '\n');
		if (msg_end == NULL)
			return 0;
		if (sscanf(ibuf, "%c", &scan_ch) != 1)
			goto parse_err;
		switch (scan_ch) {
		case 'm':
			ppos = 1;
			if (sscanf(ibuf + ppos, "%p", &mem_addr) != 1)
				goto parse_err;

			if (!(pmask & PARSE_S))
				goto skip_s;
			sep_pos = strchr(ibuf + ppos, ';');
			if (!sep_pos)
				goto parse_err;
			ppos = sep_pos + 1 - ibuf;
			if (sscanf(ibuf + ppos, "%c", &scan_ch) != 1 ||
			    scan_ch != 's')
				goto parse_err;
			ppos++;
			if (sscanf(ibuf + ppos, "%zd", &mem_size) != 1)
				goto parse_err;
skip_s:
			if (!(pmask & PARSE_C))
				goto skip_c;
			sep_pos = strchr(ibuf + ppos, ';');
			if (!sep_pos)
				goto parse_err;
			ppos = sep_pos + 1 - ibuf;
			if (sscanf(ibuf + ppos, "%c", &scan_ch) != 1 ||
			    scan_ch != 'c')
				goto parse_err;
			ppos++;
			if (sscanf(ibuf + ppos, "%p", &code_addr) != 1)
				goto parse_err;
skip_c:
			if (!(pmask & PARSE_O))
				goto skip_o;
			sep_pos = strchr(ibuf + ppos, ';');
			if (!sep_pos)
				goto parse_err;
			ppos = sep_pos + 1 - ibuf;
			if (sscanf(ibuf + ppos, "%c", &scan_ch) != 1 ||
			    scan_ch != 'o')
				goto parse_err;
			ppos++;
			if (sscanf(ibuf + ppos, "%p", &stack_offs) != 1)
				goto parse_err;
skip_o:
			pp.ibuf = ibuf;
			pp.ppos = ppos;
			pp.argp = argp;

			// call post parsing function
			mf(cfg, &pp, heap_start, mem_addr, mem_size,
			   code_addr, stack_offs);
			break;
		case 'f':
			if (!ff)
				break;
			ppos = 1;
			if (sscanf(ibuf + ppos, "%p", &mem_addr) != 1)
				goto parse_err;

			// call post parsing function
			ff(cfg, argp, mem_addr);
			break;
		case 'h':
			ppos = 1;
			if (sscanf(ibuf + ppos, "%p", &heap_start) != 1)
				goto parse_err;
			break;
		}
		// prepare for next msg parsing

		tmp_ilen = msg_end + 1 - ibuf;
		// move rest to the front
		memmove(ibuf, ibuf + tmp_ilen, ilen - tmp_ilen);
		// zero what's behind the rest
		memset(ibuf + ilen - tmp_ilen, 0, tmp_ilen);
		ilen -= tmp_ilen;
		ipos -= tmp_ilen;
		//cout << "ilen: " << ilen << " ipos " << ipos << endl;

		goto next;
	} else if (argp) {
		return 1;
	}
	return 0;

parse_err:
	cerr << "parse error at ppos: " << ppos << endl;
	cerr << ibuf;
	memset(ibuf, 0, sizeof(ibuf));
	ilen = 0;
	ipos = 0;
	return 0;
}
