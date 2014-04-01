/* discovery.cpp:    discover dynamic memory objects
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
 *
 * By the original authors of ugtrain there shall be ABSOLUTELY
 * NO RESPONSIBILITY or LIABILITY for derived work and/or abusive
 * or malicious use. The ugtrain is an education project and
 * sticks to the copyright law by providing configs for games
 * which ALLOW CHEATING only. We can't and we don't accept abusive
 * configs or codes which might turn ugtrain into a cracker tool!
 */

#include <vector>
#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include "common.h"
#include "commont.cpp"
#include "getch.h"
#include "cfgparser.h"
#include "fifoparser.h"
#include "system.h"
#include "discovery.h"
#include "adaption.h"

#define DYNMEM_FILE "/tmp/memhack_file"
#define DISASM_FILE "/tmp/memhack_disasm"
#define MAX_BT 11
#define DISC_DEBUG 0


static void process_stage5_result (DynMemEntry *dynmem)
{
	cout << "Class " << dynmem->name << ":" << endl;
	cout << "old_offs: " << hex << dynmem->stack_offs
	     << ", new_offs: " << dynmem->adp_soffs << dec << endl;
}

static i32 postproc_stage5 (struct app_options *opt, list<CfgEntry> *cfg,
			    vector<string> *lines)
{
	list<CfgEntry>::iterator cfg_it;
	DynMemEntry *tmp = NULL;
	bool discovered = false;
	char ch;
	i32 ret;

	list_for_each (cfg, cfg_it) {
		if (!cfg_it->dynmem || cfg_it->dynmem == tmp)
			continue;
		tmp = cfg_it->dynmem;

		if (tmp->adp_addr && opt->disc_addr == tmp->adp_addr &&
		    tmp->adp_soffs) {
			process_stage5_result(tmp);
			discovered = true;
			continue;
		}
		if (!(tmp->adp_addr && tmp->adp_soffs)) {
			if (discovered) {
				cout << "Continue with class " << tmp->name
				     << " (y/n)? : ";
				fflush(stdout);
				ch = 'n';
				ch = do_getch();
				cout << ch << endl;
				if (ch == 'y') {
					opt->disc_str[1] = '\0';
					return DISC_NEXT;
				} else {
					goto out_wb;
				}
			}
			cerr << "Discovery failed!" << endl;
			return -1;
		}
	}
	cout << "Discovery successful!" << endl;

out_wb:
	// Take over discovery
	ret = take_over_config(opt, cfg, lines);
	if (ret)
		return ret;
	return DISC_OKAY;
}

// parameter for process_disc1234_malloc()
struct disc_pp {
	void *in_addr;
	struct app_options *opt;
	bool do_disasm;
};

// mf() callback for read_dynmem_buf()
static void process_disc1234_malloc (list<CfgEntry> *cfg,
				     struct post_parse *pp,
				     void *heap_start,
				     void *mem_addr,
				     size_t mem_size,
				     void *code_addr,
				     void *stack_offs)
{
	struct disc_pp *dpp = (struct disc_pp *) pp->argp;
	void *in_addr = dpp->in_addr;
	char stage = dpp->opt->disc_str[0];
	bool *do_disasm = &dpp->do_disasm;
	void *codes[MAX_BT] = { NULL };
	void *soffs[MAX_BT] = { NULL };
	char *sep_pos;
	i32 i, ret, num_codes = 0;
	string cmd_str, tmp_str;
	char pbuf[PIPE_BUF] = { 0 };
	ssize_t rbytes = 0;

	if (in_addr >= mem_addr &&
	    in_addr < PTR_ADD(void *,mem_addr, mem_size)) {
		cout << "m" << mem_addr << ";" << "s" << mem_size
		     << " contains " << in_addr << ", offs: "
		     << PTR_SUB(void *, in_addr, mem_addr) << ", heap offs: "
		     << PTR_SUB(void *, mem_addr, heap_start) << endl;

		switch (stage) {
		case '3':
		case '4':
			sep_pos = strchr(pp->ibuf + pp->ppos, ';');
			if (!sep_pos)
				return;
			pp->ppos = sep_pos + 1 - pp->ibuf;
			break;
		default:
			return;
		}

		/* stage 3 and 4 parsing */
		for (i = 0; i < MAX_BT; i++) {
			ret = sscanf(pp->ibuf + pp->ppos, "c%p;o%p",
				     &codes[i], &soffs[i]);
			if (ret >= 1) {
				num_codes++;

				sep_pos = strchr(pp->ibuf + pp->ppos, ';');
				if (!sep_pos)
					break;
				pp->ppos = sep_pos + 1 - pp->ibuf;
			} else {
				break;
			}
			if (ret == 2) {
				sep_pos = strchr(pp->ibuf + pp->ppos, ';');
				if (!sep_pos)
					break;
				pp->ppos = sep_pos + 1 - pp->ibuf;
			}
		}

		/* stage 3 and 4 post-processing */
		// get disassembly only once
		if (*do_disasm) {
			cmd_str = "objdump -D ";
			cmd_str += dpp->opt->game_binpath;
			cmd_str += " > " DISASM_FILE;
#if (DISC_DEBUG)
			cout << "$ " << cmd_str << endl;
#endif
			ret = run_cmd(cmd_str.c_str(), NULL);
			if (ret < 0)
				return;
			*do_disasm = false;
		}
		for (i = 0; i < num_codes; i++) {
			// get the function call from disassembly
			tmp_str = to_string(codes[i]);

			cmd_str = "cat " DISASM_FILE " | grep -B 1 -e \"^[ ]\\+";
			cmd_str += tmp_str.substr(2, string::npos);  // no '0x'
			cmd_str += ":\" | head -n 1 | grep -o -e \"call.*$\" "
				   "| grep -o -e \"<.*@plt>\"";
#if (DISC_DEBUG)
			cout << "$ " << cmd_str << endl;
#endif
			rbytes = run_cmd_pipe(cmd_str.c_str(), NULL, pbuf,
					      sizeof(pbuf));
			if (rbytes > 0) {
				// remove trailing new line
				if (pbuf[rbytes - 1] == '\n')
					pbuf[rbytes - 1] = '\0';
			}

			// output one call from backtrace
			cout << "c" << codes[i];
			if (stage == '4')
				cout << ";o" << soffs[i];
			cout << " " << pbuf << endl;

			// cleanup pipe buffer
			if (rbytes > 0)
				memset(pbuf, 0, rbytes);
		}
	}
}

static i32 postproc_stage1234 (struct app_options *opt, list<CfgEntry> *cfg)
{
	i32 ifd, pmask = PARSE_M | PARSE_S;
	void *mem_addr;
	struct disc_pp dpp;

	ifd = open(DYNMEM_FILE, O_RDONLY);
	if (ifd < 0) {
		perror("open ifd");
		return -1;
	}

	restore_getch();

	cout << "Memory address (e.g. 0xdeadbeef): ";
	fflush(stdout);
	cin >> hex >> mem_addr;
	if (mem_addr == NULL) {
		cerr << "Error: Invalid memory address!" << endl;
		return -1;
	}
	cout << hex << "Searching reverse for " << mem_addr << dec
	     << " in discovery output.." << endl;

	dpp.in_addr = mem_addr;
	dpp.opt = opt;
	dpp.do_disasm = true;

	while (true) {
		if (read_dynmem_buf(cfg, &dpp, ifd, pmask, true,
		    process_disc1234_malloc, NULL) < 0)
			break;
	}
	rm_file(DISASM_FILE);

	if (prepare_getch() != 0) {
		cerr << "Error while terminal preparation!" << endl;
		return -1;
	}

	return 0;
}

i32 postproc_discovery (struct app_options *opt, list<CfgEntry> *cfg,
			vector<string> *lines)
{
	if (opt->disc_str[0] >= '1' && opt->disc_str[0] <= '4')
		return postproc_stage1234(opt, cfg);
	if (opt->disc_str[0] != '5')
		return -1;
	return postproc_stage5(opt, cfg, lines);
}

// mf() callback for read_dynmem_buf()
static void process_disc5_output (list<CfgEntry> *cfg,
				  struct post_parse *pp,
				  void *heap_start,
				  void *mem_addr,
				  size_t mem_size,
				  void *code_addr,
				  void *stack_offs)
{
	list<CfgEntry>::iterator it;

	cout << "Discovery output: " << endl;
	cout << "m" << hex << mem_addr << dec << ";s"
	     << mem_size << hex << ";c" << code_addr
	     << ";o" << stack_offs << dec << endl;

	// find object and set adp_soffs
	list_for_each (cfg, it) {
		if (it->dynmem &&
		    it->dynmem->adp_addr == code_addr) {
			if (it->dynmem->adp_soffs == stack_offs) {
				goto out;
			} else if (!it->dynmem->adp_soffs) {
				it->dynmem->adp_soffs = stack_offs;
				goto out;
			}
			break;
		}
	}
out:
	return;
}

void run_stage5_loop (list<CfgEntry> *cfg, i32 ifd, i32 pmask, pid_t pid)
{
	while (true) {
		sleep_sec(1);
		read_dynmem_buf(cfg, NULL, ifd, pmask, 0, process_disc5_output,
				NULL);
		if (!pid_is_running(pid, pid, NULL, true))
			break;
	}
}

#define DEBUG_PARSING 0

void run_stage1234_loop (void *argp)
{
	i32 ifd = *(i32 *) argp;
	i32 ofd;
	char buf[PIPE_BUF];
	ssize_t rbytes, wbytes;

#if DEBUG_PARSING
	ofd = open("/dev/null", O_WRONLY);
#else
	ofd = open(DYNMEM_FILE, O_WRONLY | O_CREAT | O_TRUNC,
		   0644);
#endif
	if (ofd < 0) {
		perror("open ofd");
		exit(1);
	}

	while (true) {
		rbytes = read(ifd, buf, sizeof(buf));
		if (rbytes == 0 || (rbytes < 0 && errno == EAGAIN))
			continue;
		if (rbytes < 0) {
			perror("read");
			break;
		}
		wbytes = write(ofd, buf, rbytes);
		if (wbytes != rbytes) {
			perror("write");
			break;
		}
	}
}

i32 prepare_discovery (struct app_options *opt, list<CfgEntry> *cfg)
{
	string disc_str, cmd_str;
	char *iptr, *main_part;
	i32 i, ret, ioffs = 0;
	list<CfgEntry>::iterator it;
	void *heap_soffs, *heap_eoffs, *bt_saddr, *bt_eaddr;
	ulong mem_size;
	char pbuf[PIPE_BUF] = { 0 };
	char gbt_buf[sizeof(GBT_CMD)] = { 0 };
	bool use_gbt = false;

	if (!opt->disc_str)
		return 0;

	if (opt->disc_str[0] == 'p') {
		iptr = strstr(opt->disc_str, ";;");
		if (!iptr)
			goto err;
		opt->disc_offs = iptr - opt->disc_str + 2;
	}

	main_part = opt->disc_str + opt->disc_offs;
	switch (main_part[0]) {
	case '0':
		// just get stack end and heap start
		opt->disc_str = (char *) "0";
		opt->disc_offs = 0;
		break;
	case '1':
	case '2':
		if (strlen(main_part) == 1) {
			disc_str = opt->disc_str;
			disc_str += ";0x0;0x0;0";
			opt->disc_str = to_c_str(&disc_str);
		}
		cout << "disc_str: " << opt->disc_str << endl;
		break;
	case '3':
		ret = sscanf(&main_part[1], ";%3s;", gbt_buf);
		if (ret == 1 &&
		    strncmp(gbt_buf, GBT_CMD, sizeof(GBT_CMD) - 1) == 0) {
			use_gbt = true;
			ioffs = sizeof(GBT_CMD);
		}
	case '4':
		ret = sscanf(&main_part[1] + ioffs, ";%p;%p;%lu;%p;%p",
			     &heap_soffs, &heap_eoffs, &mem_size, &bt_saddr,
			     &bt_eaddr);
		if (ret < 3) {
			cerr << "Error: Not enough arguments for discovery "
				"stage " << main_part[0] << "!" << endl;
			cerr << "Use at least \'" << main_part[0]
			     << ";0x0;0x0;<size>\'" << endl;
			goto err;
		}
		if (mem_size == 0) {
			cerr << "Error: Too much data! Please discover the "
				"size first!" << endl;
			goto err;
		}
		if (ret < 5) {
			cmd_str = "objdump -p ";
			cmd_str += opt->game_binpath;
			cmd_str += " | grep \"INIT\\|FINI\" "
				   "| tr -d [:upper:] | tr -d [:blank:]";
			cout << "$ " << cmd_str << endl;
			if (run_cmd_pipe(cmd_str.c_str(), NULL, pbuf,
			    sizeof(pbuf)) <= 0)
				goto err;
			if (sscanf(pbuf, "%p\n%p", &bt_saddr, &bt_eaddr) != 2) {
				goto err;
			} else {
				main_part[1] = '\0';
				disc_str = opt->disc_str;
				if (use_gbt)
					disc_str += ";" GBT_CMD ";";
				else
					disc_str += ";";
				disc_str += to_string(heap_soffs);
				disc_str += ";";
				disc_str += to_string(heap_eoffs);
				disc_str += ";";
				disc_str += to_string(mem_size);
				disc_str += ";";
				disc_str += to_string(bt_saddr);
				disc_str += ";";
				disc_str += to_string(bt_eaddr);
				opt->disc_str = to_c_str(&disc_str);
			}
		}
		cout << "disc_str: " << opt->disc_str << endl;
		break;
	case '5':
		if (!opt->do_adapt) {
			list_for_each (cfg, it) {
				if (it->dynmem && !it->dynmem->adp_addr)
					it->dynmem->adp_addr = it->dynmem->code_addr;
			}
		}
		if (strlen(opt->disc_str) != 1) {
			cerr << "Sorry, but this stage is reserved for "
				"auto-adaption!" << endl;
			goto err;
		} else {
			list_for_each (cfg, it) {
				if (it->dynmem && it->dynmem->adp_addr &&
				    !it->dynmem->adp_soffs) {
					opt->disc_addr = it->dynmem->adp_addr;
					goto found;
				}
			}
			goto err;
found:
			disc_str = opt->disc_str[0];
			disc_str += ";0x0;0x0;";
			disc_str += to_string(it->dynmem->mem_size);
			for (i = 0; i < 3; i++) {
				disc_str += ";";
				disc_str += to_string(it->dynmem->adp_addr);
			}
			opt->disc_str = to_c_str(&disc_str);
			cout << "Discovering class " << it->dynmem->name
			     << " stack offset." << endl
			     << "Please ensure that such objects get "
				"allocated!" << endl
			     << "disc_str: " << opt->disc_str << endl;
		}
		break;
	default:
		goto err;
	}

	return 0;
err:
	cerr << "Error while preparing discovery!" << endl;
	return -1;
}
