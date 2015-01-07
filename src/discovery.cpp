/* discovery.cpp:    discover dynamic memory objects
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

// local includes
#include <lib/getch.h>
#include <lib/system.h>
#include <adaption.h>
#include <cfgparser.h>
#include <common.h>
#include <commont.cpp>
#include <discovery.h>
#include <fifoparser.h>

#define DISASM_FILE "/tmp/memhack_disasm"
#define MAX_BT 11
#define DISC_DEBUG 0


static void process_stage5_result (DynMemEntry *dynmem)
{
	cout << "Class " << dynmem->name << ":" << endl;
	cout << "old_offs: 0x" << hex << dynmem->stack_offs
	     << ", new_offs: 0x" << dynmem->adp_soffs << dec << endl;
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
	ptr_t in_addr;
	struct app_options *opt;
	bool do_disasm;
};

// mf() callback for read_dynmem_buf()
static void process_disc1234_malloc (MF_PARAMS)
{
	struct disc_pp *dpp = (struct disc_pp *) pp->argp;
	ptr_t in_addr = dpp->in_addr;
	struct app_options *opt = dpp->opt;
	char stage = opt->disc_str[0];
	bool *do_disasm = &dpp->do_disasm;
	ptr_t codes[MAX_BT] = { 0 };
	ptr_t soffs[MAX_BT] = { 0 };
	char *sep_pos;
	i32 i, ret, num_codes = 0;
	string cmd_str, tmp_str;
	char pbuf[PIPE_BUF] = { 0 };
	ssize_t rbytes = 0;

	if (in_addr >= mem_addr &&
	    in_addr < mem_addr + mem_size) {
		cout << "m0x" << hex << mem_addr << dec << ";" << "s"
		     << mem_size << " contains 0x" << hex << in_addr
		     << ", offs: 0x" << in_addr - mem_addr
		     << ", heap offs: 0x" << mem_addr - heap_start
		     << dec << endl;

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
			ret = sscanf(pp->ibuf + pp->ppos, "c" SCN_PTR ";o" SCN_PTR,
				     &codes[i], &soffs[i]);
			if (ret >= 1) {
				num_codes++;
				codes[i] = codes[i] - opt->code_offs;

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
		if (*do_disasm && opt->have_objdump) {
			cmd_str = "objdump -D ";
			cmd_str += opt->game_binpath;
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
			if (!opt->have_objdump)
				goto skip_lookup;
			// get the function call from disassembly
			tmp_str = to_xstring(codes[i]);

			cmd_str = "cat " DISASM_FILE " | grep -B 1 -e \"^[ ]\\+";
			cmd_str += tmp_str;
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
skip_lookup:
			// output one call from backtrace
			cout << "c0x" << hex << codes[i];
			if (stage == '4')
				cout << ";o0x" << hex << soffs[i] << dec;
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
	ptr_t mem_addr;
	struct disc_pp dpp;

	ifd = open(opt->dynmem_file, O_RDONLY);
	if (ifd < 0) {
		perror("open ifd");
		return -1;
	}

	restore_getch();

	cout << "Memory address (e.g. 0xdeadbeef): ";
	fflush(stdout);
	cin >> hex >> mem_addr;
	if (!mem_addr) {
		cerr << "Error: Invalid memory address!" << endl;
		return -1;
	}
	cout << hex << "Searching reverse for 0x" << mem_addr << dec
	     << " in discovery output.." << endl;

	dpp.in_addr = mem_addr;
	dpp.opt = opt;
	dpp.do_disasm = true;

	while (true) {
		if (read_dynmem_buf(cfg, &dpp, ifd, pmask, true,
		    opt->code_offs, process_disc1234_malloc, NULL) < 0)
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
static void process_disc5_output (MF_PARAMS)
{
	list<CfgEntry>::iterator it;

	code_addr -= code_offs;
	cout << "Discovery output: " << endl;
	cout << "m0x" << hex << mem_addr << dec << ";s"
	     << mem_size << ";c0x" << hex << code_addr
	     << ";o0x" << stack_offs << dec << endl;

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

void run_stage5_loop (list<CfgEntry> *cfg, i32 ifd, i32 pmask, pid_t pid,
		      ptr_t code_offs)
{
	enum pstate pstate;
	while (true) {
		sleep_sec_unless_input(1, ifd, -1);
		read_dynmem_buf(cfg, NULL, ifd, pmask, 0, code_offs,
				process_disc5_output, NULL);
		pstate = check_process(pid, NULL);
		if (pstate == PROC_DEAD || pstate == PROC_ZOMBIE)
			break;
	}
}

/*
 * worker process
 * changed values aren't available in the parent
 */
void run_stage1234_loop (void *argp)
{
	struct disc_loop_pp *dpp = (struct disc_loop_pp *) argp;
	i32 ifd = dpp->ifd;
	struct app_options *opt = dpp->opt;
	i32 ofd;
	char buf[PIPE_BUF];
	ssize_t rbytes, wbytes;

	ofd = open(opt->dynmem_file, O_WRONLY | O_CREAT | O_TRUNC,
		   0644);

	if (ofd < 0) {
		perror("open ofd");
		exit(1);
	}

	while (true) {
		sleep_sec_unless_input(1, ifd, -1);
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
	char *pos, *disc_part;
	char stage;
	i32 i, ret;
	list<CfgEntry>::iterator it;
	ptr_t heap_soffs = 0, heap_eoffs = 0;
	ptr_t bt_saddr = 0, bt_eaddr = 0, code_addr = 0;
	ulong mem_size;
	char pbuf[PIPE_BUF] = { 0 };
	bool use_gbt = false;

	if (!opt->disc_str)
		return 0;

	if (opt->disc_str[0] == 'p') {
		disc_part = opt->disc_str + 1;
		pos = strstr(disc_part, ";;");
		if (!pos)
			goto err;
		opt->disc_offs = pos - opt->disc_str + 2;
	}

	disc_part = opt->disc_str + opt->disc_offs;
	stage = disc_part[0];
	switch (stage) {
	case '0':
		// just get stack end and heap start
		opt->disc_str = (char *) "0";
		opt->disc_offs = 0;
		break;
	case '1':
		// fall through
	case '2':
		disc_part++;
		if (disc_part[0] == '\0') {
			disc_str = opt->disc_str;
			disc_str += ";0x0;0x0;0";
			opt->disc_str = to_c_str(&disc_str);
		} else {
			ret = sscanf(disc_part, ";" SCN_PTR ";" SCN_PTR ";%lu;",
				&heap_soffs, &heap_eoffs, &mem_size);
			if (ret == 0 && stage == '2') {
				ret = sscanf(disc_part, ";%lu;", &mem_size);
				if (ret == 1)
					ret += 2;
			}
			if (ret < 2) {
				cerr << "Syntax error in discovery string!"
				     << endl;
				cerr << "disc_str: " << opt->disc_str << endl;
				goto err;
			}
		}
		cout << "disc_str: " << opt->disc_str << endl;
		break;
	case '3':
		ret = strncmp(&disc_part[1], ";" GBT_CMD ";",
			sizeof(GBT_CMD) + 1);
		if (ret == 0) {
			use_gbt = true;
			disc_part += sizeof(GBT_CMD);
		}
		// fall through
	case '4':
		disc_part++;
		ret = sscanf(disc_part, ";" SCN_PTR ";" SCN_PTR ";%lu;" SCN_PTR
			     ";" SCN_PTR ";" SCN_PTR, &heap_soffs, &heap_eoffs,
			     &mem_size, &bt_saddr, &bt_eaddr, &code_addr);
		if (ret == 0) {
			ret = sscanf(disc_part, ";%lu;" SCN_PTR ";" SCN_PTR ";"
				     SCN_PTR, &mem_size, &bt_saddr, &bt_eaddr,
				     &code_addr);
			if (ret >= 1)
				ret += 2;
		}
		opt->code_addr = code_addr;
		if (ret < 3) {
			cerr << "Error: Not enough arguments for discovery "
				"stage " << stage << "!" << endl;
			cerr << "Use at least \'" << stage
			     << ";<size>\'" << endl;
			goto err;
		}
		if (mem_size == 0) {
			cerr << "Error: Too much data! Please discover the "
				"size first!" << endl;
			goto err;
		}
		if (ret < 5) {
			if (!opt->have_objdump)
				goto err;
			cmd_str = "objdump -p ";
			cmd_str += opt->game_binpath;
			cmd_str += " | grep \"INIT\\|FINI\" "
				   "| tr -d [:upper:] | tr -d [:blank:] | head -n 2";
			cout << "$ " << cmd_str << endl;
			if (run_cmd_pipe(cmd_str.c_str(), NULL, pbuf,
			    sizeof(pbuf)) <= 0)
				goto err;
			if (sscanf(pbuf, SCN_PTR "\n" SCN_PTR, &bt_saddr, &bt_eaddr) != 2) {
				goto err;
			} else {
				opt->disc_str[opt->disc_offs + 1] = '\0';
				disc_str = opt->disc_str;
				if (use_gbt)
					disc_str += ";" GBT_CMD ";0x";
				else
					disc_str += ";0x";
				disc_str += to_xstring(heap_soffs);
				disc_str += ";0x";
				disc_str += to_xstring(heap_eoffs);
				disc_str += ";";
				disc_str += to_string(mem_size);
				disc_str += ";0x";
				disc_str += to_xstring(bt_saddr);
				disc_str += ";0x";
				disc_str += to_xstring(bt_eaddr);
				opt->disc_str = to_c_str(&disc_str);
			}
		}
		cout << "disc_str: " << opt->disc_str << endl;
		break;
	case '5':
		if (!opt->do_adapt) {
			list_for_each (cfg, it) {
				if (it->dynmem && !it->dynmem->adp_addr) {
					it->dynmem->adp_size = it->dynmem->mem_size;
					it->dynmem->adp_addr = it->dynmem->code_addr;
				}
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
			disc_str += to_string(it->dynmem->adp_size);
			for (i = 0; i < 3; i++) {
				disc_str += ";0x";
				disc_str += to_xstring(it->dynmem->adp_addr);
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
