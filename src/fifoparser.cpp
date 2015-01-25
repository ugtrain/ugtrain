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

#include <cstring>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

// local includes
#include <fifoparser.h>

#define REVERSE_FST -2  // reverse file read start
#define REVERSE_EOF -3  // reverse file read end


ssize_t read_dynmem_buf (list<CfgEntry> *cfg, void *argp, i32 ifd, i32 pmask,
			 bool reverse, ptr_t code_offs, struct parse_cb *pcb)
{
	ptr_t mem_addr = 0, code_addr = 0, stack_offs = 0;
	static ptr_t heap_start = 0;
	static ssize_t ipos = 0, ilen = 0;
	ulong mem_size = 0;
	ssize_t tmp_ilen, ppos = 0;
	static char ibuf[PIPE_BUF] = { 0 };
	char *msg_start = ibuf, *msg_end = ibuf, *sep_pos = ibuf;
	char *lib_name = NULL;
	char scan_ch;
	struct post_parse pp;
	static off_t fd_offs = REVERSE_FST;

	if (reverse) {
		if (fd_offs == REVERSE_FST) {
			// read heap start from file first
			tmp_ilen = read(ifd, ibuf, 50);
			if (tmp_ilen <= 0 && argp)
				return -1;
			if (sscanf(msg_start, "%c", &scan_ch) != 1)
				goto parse_err;
			if (scan_ch == 'h') {
				ppos = msg_start - ibuf + 1;
				if (sscanf(ibuf + ppos, SCN_PTR, &heap_start) != 1)
					goto parse_err;
			}
			// clear buffer again
			memset(ibuf, 0, tmp_ilen);
			// set file pointer to file end
			fd_offs = lseek(ifd, 0, SEEK_END);
		}
		if (fd_offs < 0)
			return -1;
		// we use ipos as read length here
		ipos = fd_offs;
		if ((unsigned) ipos > sizeof(ibuf) - 1 - ilen) {
			ipos = sizeof(ibuf) - 1 - ilen;
			fd_offs = lseek(ifd, fd_offs - ipos, SEEK_SET);
			if (fd_offs < 0)
				return -1;
		} else {
			fd_offs = lseek(ifd, 0, SEEK_SET);
			if (fd_offs < 0)
				return -1;
			fd_offs = REVERSE_EOF;
		}
		// move rest to the end
		if (ilen)
			memmove(ibuf + ipos, ibuf, ilen);

		// read from FIFO and concat. incomplete msgs
		tmp_ilen = read(ifd, ibuf, ipos);  // always '\0' at end
	} else {
		// read from FIFO and concat. incomplete msgs
		tmp_ilen = read(ifd, ibuf + ipos, sizeof(ibuf) - 1 - ilen);  // always '\0' at end
	}
	if (tmp_ilen > 0) {
		//cout << "ibuf: " << string(ibuf) << endl;
		ilen += tmp_ilen;
		//cout << "ilen: " << ilen << " tmp_ilen " << tmp_ilen << endl;
		//cout << "ipos: " << ipos << endl;
		ipos += tmp_ilen;

		// parse messages
next:
		if (reverse) {
			msg_end = strrchr(ibuf, '\n');
			if (msg_end == NULL)
				return tmp_ilen;
			else
				*msg_end = '\0';
			msg_start = strrchr(ibuf, '\n');
			if (msg_start == NULL) {
				// incomplete message, restore message end
				*msg_end = '\n';
				return tmp_ilen;
			} else {
				msg_start += 1;
			}
		} else {
			msg_end = strchr(ibuf, '\n');
			if (msg_end == NULL)
				return tmp_ilen;
		}

		if (sscanf(msg_start, "%c", &scan_ch) != 1)
			goto parse_err;
		switch (scan_ch) {
		case 'm':
			if (!pcb || !pcb->mf)
				break;
			ppos = msg_start - ibuf + 1;
			if (sscanf(ibuf + ppos, SCN_PTR, &mem_addr) != 1)
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
			if (sscanf(ibuf + ppos, "%lu", &mem_size) != 1)
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
			if (sscanf(ibuf + ppos, SCN_PTR, &code_addr) != 1)
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
			if (sscanf(ibuf + ppos, SCN_PTR, &stack_offs) != 1)
				goto parse_err;
skip_o:
			pp.ibuf = ibuf;
			pp.ppos = ppos;
			pp.argp = argp;

			// call post parsing function
			pcb->mf(cfg, &pp, heap_start, mem_addr, mem_size,
				code_offs, code_addr, stack_offs);
			break;
		case 'f':
			if (!pcb || !pcb->ff)
				break;
			ppos = msg_start - ibuf + 1;
			if (sscanf(ibuf + ppos, SCN_PTR, &mem_addr) != 1)
				goto parse_err;

			// call post parsing function
			pcb->ff(cfg, argp, mem_addr);
			break;
		case 'l':
			if (!pcb || !pcb->lf)
				break;
			ppos = msg_start - ibuf + 1;
			if (ibuf[ppos] != ';')
				goto parse_err;
			ppos++;
			lib_name = new char[msg_end - (ibuf + ppos) + 1];
			memcpy(lib_name, ibuf + ppos, msg_end - (ibuf + ppos));
			lib_name[msg_end - (ibuf + ppos)] = '\0';

			// call post parsing function
			pcb->lf(argp, lib_name);
			delete lib_name;
			break;
		case 'h':
			ppos = msg_start - ibuf + 1;
			if (sscanf(ibuf + ppos, SCN_PTR, &heap_start) != 1)
				goto parse_err;
			break;
		}

		// prepare for next msg parsing
		if (reverse) {
			// zero parsed message
			tmp_ilen = msg_end - msg_start + 1;
			memset(msg_start, 0, tmp_ilen);
			ilen -= tmp_ilen;
			ipos = 0;
		} else {
			tmp_ilen = msg_end + 1 - ibuf;
			// move rest to the front
			memmove(ibuf, ibuf + tmp_ilen, ilen - tmp_ilen);
			// zero what's behind the rest
			memset(ibuf + ilen - tmp_ilen, 0, tmp_ilen);
			ilen -= tmp_ilen;
			ipos -= tmp_ilen;
			//cout << "ilen: " << ilen << " ipos " << ipos << endl;
		}

		goto next;
	} else if (argp) {
		return -1;
	}
	return tmp_ilen;

parse_err:
	cerr << "parse error at ppos: " << ppos << endl;
	cerr << ibuf;
	memset(ibuf, 0, sizeof(ibuf));
	ilen = 0;
	ipos = 0;
	return 0;
}
