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
#include "memattach.h"
#include "system.h"
#include "discovery.h"

#define DYNMEM_FILE "/tmp/memhack_file"
#define MAX_BT 11


static void process_stage5_result (DynMemEntry *dynmem)
{
	u32 i;
	char ch;

	cout << "Class " << dynmem->name
	     << ", old_code: " << hex << dynmem->code_addr
	     << ", new_code: " << dynmem->adp_addr << dec << endl;
	for (i = 0; i < dynmem->num_stack; i++) {
		cout << i << ": old_offs: " << hex << dynmem->stack_offs[i]
		     << ", new_offs: " << dynmem->adp_soffs[i] << dec
		     << (dynmem->soffs_ign[i] ? ", ignored" : "") << endl;
	}
	cout << "Okay this way (y/n)? : ";
	fflush(stdout);
	ch = 'n';
	ch = do_getch();
	cout << ch << endl;
	if (ch != 'y') {
		cerr << "Discovery failed!" << endl;
		exit(-1);
	}
}

static void swap_adp_soffs (DynMemEntry *dynmem, u32 idx1, u32 idx2)
{
	void *tmp_adp_soffs;

	tmp_adp_soffs = dynmem->adp_soffs[idx1];
	dynmem->adp_soffs[idx1] = dynmem->adp_soffs[idx2];
	dynmem->adp_soffs[idx2] = tmp_adp_soffs;
}

static void swap_stack_offsets (DynMemEntry *dynmem, u32 idx1, u32 idx2)
{
	void *tmp_stack_offs;
	bool tmp_soffs_ign;
	u32 tmp_cfg_line;

	tmp_stack_offs = dynmem->stack_offs[idx1];
	tmp_soffs_ign = dynmem->soffs_ign[idx1];
	tmp_cfg_line = dynmem->cfg_lines[idx1];
	dynmem->stack_offs[idx1] = dynmem->stack_offs[idx2];
	dynmem->soffs_ign[idx1] = dynmem->soffs_ign[idx2];
	dynmem->cfg_lines[idx1] = dynmem->cfg_lines[idx2];
	dynmem->stack_offs[idx2] = tmp_stack_offs;
	dynmem->soffs_ign[idx2] = tmp_soffs_ign;
	dynmem->cfg_lines[idx2] = tmp_cfg_line;
}

static void bubsort_stack_offs (void **arr, u32 size, DynMemEntry *dynmem,
				void (*swap)(DynMemEntry *dynmem, u32, u32))
{
	u32 i, n, m;

	n = size;
	do {
		for (i = 1, m = 0; i < n; i++) {
			if (arr[i] == NULL)
				break;
			if ((ulong) arr[i - 1] > (ulong) arr[i]) {
				swap(dynmem, i - 1, i);
				m = i;
			}
		}
		n = m;
	} while (n > 0);
}

/*
 * Sort stack offsets so that adaption order becomes
 * independent from the order in the config.
 * We use simple bubble sort here.
 */
static void sort_stack_offsets (DynMemEntry *dynmem)
{
	bubsort_stack_offs(dynmem->stack_offs, MAX_STACK, dynmem,
			   swap_stack_offsets);

	bubsort_stack_offs(dynmem->adp_soffs, MAX_STACK, dynmem,
			   swap_adp_soffs);
}

static i32 postproc_stage5 (struct app_options *opt, list<CfgEntry> *cfg,
			    string *cfg_path, vector<string> *lines)
{
	list<CfgEntry>::iterator cfg_it;
	DynMemEntry *tmp = NULL;
	bool discovered = false;
	u32 lnr, i;
	char ch;

	for (cfg_it = cfg->begin(); cfg_it != cfg->end(); cfg_it++) {
		if (!cfg_it->dynmem || cfg_it->dynmem == tmp)
			continue;
		tmp = cfg_it->dynmem;

		if (tmp->discovered) {
			discovered = true;
			continue;
		}
		if (tmp->adp_addr == opt->disc_addr && !tmp->adp_failed &&
		    tmp->adp_sidx >= tmp->num_stack - tmp->num_sign) {
			sort_stack_offsets(tmp);
			if (tmp->num_stack > 1)
				process_stage5_result(tmp);
			tmp->discovered = true;
			discovered = true;
			continue;
		}
		if (!(tmp->adp_addr && !tmp->adp_failed &&
		      tmp->adp_sidx >= tmp->num_stack - tmp->num_sign)) {
			cout << "Undiscovered class(es) found!" << endl;
			if (discovered) {
				cout << "Next discovery run (y/n)? : ";
				fflush(stdout);
				ch = 'n';
				ch = do_getch();
				cout << ch << endl;
				if (ch == 'y') {
					opt->disc_str[1] = '\0';
					return DISC_NEXT;
				}
			}
			cerr << "Discovery failed!" << endl;
			exit(-1);
		}
	}
	cout << "Discovery successful!" << endl;

	// Take over discovery
	tmp = NULL;
	for (cfg_it = cfg->begin(); cfg_it != cfg->end(); cfg_it++) {
		if (!cfg_it->dynmem || cfg_it->dynmem == tmp)
			continue;
		tmp = cfg_it->dynmem;
		tmp->code_addr = tmp->adp_addr;
		for (i = 0; i < tmp->adp_sidx; i++) {
			tmp->stack_offs[i] = tmp->adp_soffs[i];
			lnr = tmp->cfg_lines[i];
			if (lnr == tmp->first_line)
				lines->at(lnr) = "dynmemstart " + tmp->name + " "
					+ to_string(tmp->mem_size) + " "
					+ to_string(tmp->code_addr) + " "
					+ to_string(tmp->stack_offs[i]);
			else if (tmp->soffs_ign[i])
				lines->at(lnr) = "dynmemign "
					+ to_string(tmp->stack_offs[i]);
			else
				lines->at(lnr) = "dynmemstack "
					+ to_string(tmp->stack_offs[i]);
		}
	}
	// Adaption isn't required anymore
	lnr = opt->adp_req_line;
	if (lnr > 0)
		lines->at(lnr) = "adapt_required 0";

	// Write back config
	cout << "Writing back config.." << endl;
	write_config_vect(cfg_path, lines);

	// Run game with libmemhack
	opt->do_adapt = 0;
	opt->disc_str = NULL;
	use_libmemhack(opt);
	return DISC_OKAY;
}

// parameter for process_disc1234_malloc()
struct disc_pp {
	void *in_addr;
	struct app_options *opt;
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
	void *codes[MAX_BT] = { NULL };
	void *soffs[MAX_BT] = { NULL };
	char *sep_pos;
	int i, ret, num_codes = 0, num_soffs = 0;
	u8 is_stage4 = 0;
	string cmd_str, tmp_str;
	char pbuf[PIPE_BUF] = { 0 };
	ssize_t rbytes = 0;

	if (in_addr >= mem_addr &&
	    in_addr < PTR_ADD(void *,mem_addr, mem_size)) {
		cout << "m" << mem_addr << ";" << "s" << mem_size
		     << " contains " << in_addr << ", offs: "
		     << PTR_SUB(void *, in_addr, mem_addr) << ", heap offs: "
		     << PTR_SUB(void *, mem_addr, heap_start) << endl;

		/* stage 3 and 4 parsing */
		for (i = 0; i < MAX_BT; i++) {
			sep_pos = strchr(pp->ibuf + pp->ppos, ';');
			if (!sep_pos)
				break;
			pp->ppos = sep_pos + 1 - pp->ibuf;
			if (is_stage4) {
				sep_pos = strchr(pp->ibuf + pp->ppos, ';');
				if (!sep_pos)
					break;
				pp->ppos = sep_pos + 1 - pp->ibuf;
			}
			ret = sscanf(pp->ibuf + pp->ppos, "c%p;o%p",
				     &codes[i], &soffs[i]);
			if (ret == 2) {
				is_stage4 = 1;
				num_codes++;
				num_soffs++;
			} else if (ret == 1) {
				if (is_stage4)
					break;
				num_codes++;
			} else {
				break;
			}
		}

		/* stage 3 and 4 post-processing */
		for (i = 0; i < num_codes; i++) {
			// get the function call from disassembly
			tmp_str = to_string(codes[i]);

			cmd_str = "objdump -D ";
			cmd_str += dpp->opt->game_path;
			cmd_str += " | grep -B 1 -e \"^[ ]\\+";
			cmd_str += tmp_str.substr(2, string::npos);  // no '0x'
			cmd_str += ":\" | head -n 1 | grep -o -e \"call.*$\" "
				   "| grep -o -e \"<.*@plt>\"";
			//cout << "$ " << cmd_str << endl;
			rbytes = run_cmd_pipe(cmd_str.c_str(), NULL, pbuf,
					      sizeof(pbuf), 1);
			if (rbytes > 0) {
				// remove trailing new line
				if (pbuf[rbytes - 1] == '\n')
					pbuf[rbytes - 1] = '\0';
			}

			// output one call from backtrace
			cout << "c" << codes[i];
			if (is_stage4)
				cout << ";o" << soffs[i];
			cout << " " << pbuf;
			cout << endl;

			// cleanup pipe buffer
			if (rbytes > 0)
				memset(pbuf, 0, rbytes);
		}
	}
	//else
		//cout << "m" << mem_addr << ";" << "s" << mem_size << endl;
}

// ff() callback for read_dynmem_buf()
static void process_disc1_free (list<CfgEntry> *cfg,
				void *argp,
				void *mem_addr)
{
	//cout << "f" << mem_addr << endl;
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

	while (1) {
		if (read_dynmem_buf(cfg, &dpp, ifd, pmask, 1,
		    process_disc1234_malloc, process_disc1_free))
			break;
	}

	if (prepare_getch() != 0) {
		cerr << "Error while terminal preparation!" << endl;
		return -1;
	}

	return 0;
}

i32 postproc_discovery (struct app_options *opt, list<CfgEntry> *cfg,
			string *cfg_path, vector<string> *lines)
{
	if (opt->disc_str[0] >= '1' && opt->disc_str[0] <= '4')
		return postproc_stage1234(opt, cfg);
	if (opt->disc_str[0] != '5')
		exit(0);
	return postproc_stage5(opt, cfg, cfg_path, lines);
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
	u32 i;

	cout << "Discovery output: " << endl;
	cout << "m" << hex << mem_addr << dec << ";s"
	     << mem_size << hex << ";c" << code_addr
	     << ";o" << stack_offs << dec << endl;

	// find object and set adp_soffs[idx]
	for (it = cfg->begin(); it != cfg->end(); it++) {
		if (it->dynmem &&
		    it->dynmem->adp_addr == code_addr) {
			for (i = 0; i < it->dynmem->num_stack; i++) {
				if (it->dynmem->adp_soffs[i] == stack_offs) {
					goto out;
				} else if (!it->dynmem->adp_soffs[i]) {
					it->dynmem->adp_soffs[i] = stack_offs;
					it->dynmem->adp_sidx++;
					goto out;
				}
			}
			// too many stack offsets
			it->dynmem->adp_failed = true;
			break;
		}
	}
out:
	return;
}

void run_stage5_loop (list<CfgEntry> *cfg, i32 ifd, i32 pmask, pid_t pid)
{
	while (1) {
		sleep_sec(1);
		read_dynmem_buf(cfg, NULL, ifd, pmask, 0, process_disc5_output,
				NULL);
		if (memattach_test(pid) != 0) {
			cerr << "PTRACE ERROR PID[" << pid << "]!"
			     << endl;
			break;
		}
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

	while (1) {
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
	int i, ret;
	list<CfgEntry>::iterator it;
	void *heap_soffs, *heap_eoffs, *bt_saddr, *bt_eaddr;
	ulong mem_size;
	char pbuf[PIPE_BUF] = { 0 };

	if (!opt->disc_str)
		return 0;

	switch (opt->disc_str[0]) {
	case '0':
		// just get stack end and heap start
		opt->disc_str = (char *) "0";
		break;
	case '1':
	case '2':
		if (strlen(opt->disc_str) == 1) {
			disc_str = opt->disc_str[0];
			disc_str += ";0x0;0x0;0";
			opt->disc_str = to_c_str(&disc_str);
		}
		cout << "disc_str: " << opt->disc_str << endl;
		break;
	case '3':
	case '4':
		ret = sscanf(&opt->disc_str[1], ";%p;%p;%lu;%p;%p", &heap_soffs,
			     &heap_eoffs, &mem_size, &bt_saddr, &bt_eaddr);
		if (ret < 3) {
			cerr << "Error: Not enough arguments for discovery "
				"stage " << opt->disc_str[0] << "!" << endl;
			cerr << "Use at least \'" << opt->disc_str[0]
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
			cmd_str += opt->game_path;
			cmd_str += " | grep \"INIT\\|FINI\" "
				   "| tr -d [:upper:] | tr -d [:blank:]";
			cout << "$ " << cmd_str << endl;
			if (run_cmd_pipe(cmd_str.c_str(), NULL, pbuf,
			    sizeof(pbuf), 1) <= 0)
				goto err;
			if (sscanf(pbuf, "%p\n%p", &bt_saddr, &bt_eaddr) != 2) {
				goto err;
			} else {
				disc_str = opt->disc_str[0];
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
			for (it = cfg->begin(); it != cfg->end(); it++) {
				if (it->dynmem && !it->dynmem->adp_addr)
					it->dynmem->adp_addr = it->dynmem->code_addr;
			}
		}
		if (strlen(opt->disc_str) != 1) {
			cerr << "Sorry, but this stage is reserved for "
				"auto-adaption!" << endl;
			goto err;
		} else {
			for (it = cfg->begin(); it != cfg->end(); it++) {
				if (it->dynmem && it->dynmem->adp_addr &&
				    it->dynmem->adp_sidx == 0) {
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
			cout << "Discovering object " << it->dynmem->name
			     << "." << endl
			     << "Please ensure that it gets allocated!" << endl;
			cout << "disc_str: " << opt->disc_str << endl;
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
