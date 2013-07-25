/* discovery.cpp:    discover dynamic memory objects
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

#include <vector>
#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include "common.h"
#include "commont.cpp"
#include "getch.h"
#include "cfgparser.h"
#include "fifoparser.h"
#include "memattach.h"
#include "discovery.h"

static i32 postproc_step4 (struct app_options *opt, list<CfgEntry> *cfg,
			   string *cfg_path, vector<string> *lines)
{
	list<CfgEntry>::iterator cfg_it;
	DynMemEntry *tmp = NULL;
	u8 discovered = 0;
	u32 lnr;
	char ch;

	for (cfg_it = cfg->begin(); cfg_it != cfg->end(); cfg_it++) {
		if (!cfg_it->dynmem)
			continue;
		if (cfg_it->dynmem->adp_addr == opt->disc_addr &&
		    cfg_it->dynmem->adp_stack) {
			discovered = 1;
			continue;
		}
		if (!(cfg_it->dynmem->adp_addr &&
		      cfg_it->dynmem->adp_stack)) {
			cout << "Undiscovered objects found!" << endl;
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
	for (cfg_it = cfg->begin(); cfg_it != cfg->end(); cfg_it++) {
		if (!cfg_it->dynmem || cfg_it->dynmem == tmp)
			continue;
		tmp = cfg_it->dynmem;
		cout << "Obj. " << tmp->name
		     << ", old_code: " << hex << tmp->code_addr
		     << ", new_code: " << tmp->adp_addr << dec << endl;
		cfg_it->dynmem->code_addr = tmp->adp_addr;
		cout << "Obj. " << tmp->name
		     << ", old_offs: " << hex << tmp->stack_offs
		     << ", new_offs: " << tmp->adp_stack << dec << endl;
		tmp->stack_offs = tmp->adp_stack;
		lnr = tmp->cfg_line;
		lines->at(lnr) = "dynmemstart " + tmp->name + " "
			+ to_string(tmp->mem_size) + " "
			+ to_string(tmp->code_addr) + " "
			+ to_string(tmp->stack_offs);
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

i32 postproc_discovery (struct app_options *opt, list<CfgEntry> *cfg,
			string *cfg_path, vector<string> *lines)
{
	if (opt->disc_str[0] != '4')
		exit(0);
	return postproc_step4(opt, cfg, cfg_path, lines);
}

// mf() callback for read_dynmem_buf()
static void process_disc_output (list<CfgEntry> *cfg,
				 void *mem_addr,
				 ssize_t mem_size,
				 void *code_addr,
				 void *stack_offs)
{
	list<CfgEntry>::iterator it;

	cout << "Discovery output: " << endl;
	cout << "m" << hex << mem_addr << dec << ";s"
	     << mem_size << hex << ";c" << code_addr
	     << ";o" << stack_offs << dec << endl;

	// find object and set adp_stack
	for (it = cfg->begin(); it != cfg->end(); it++) {
		if (it->dynmem &&
		    it->dynmem->adp_addr == code_addr) {
			it->dynmem->adp_stack = stack_offs;
			break;
		}
	}
}

void run_stage4_loop (list<CfgEntry> *cfg, i32 ifd, i32 pmask, pid_t pid)
{
	while (1) {
		sleep(1);
		read_dynmem_buf(cfg, ifd, pmask, process_disc_output,
				NULL);
		if (memattach_test(pid) != 0) {
			cerr << "PTRACE ERROR PID[" << pid << "]!"
			     << endl;
			break;
		}
	}
}

void run_stage123_loop (i32 ifd, pid_t pid)
{
	ssize_t rbytes, wbytes;
	i32 ofd;
	char buf[4096];

	//TODO fork a reader process, wait for pid to detect app exit
	ofd = open("/tmp/memhack_file", O_WRONLY | O_CREAT | O_TRUNC,
		   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
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
	string disc_str;
	int i, found = 0;
	list<CfgEntry>::iterator it;

	if (!opt->disc_str)
		return 0;

	switch (opt->disc_str[0]) {
	case '0':
		// just get stack end and heap start
		opt->disc_str = (char *) "0";
		break;
	case '1':
		if (strlen(opt->disc_str) == 1) {
			disc_str += opt->disc_str;
			disc_str += ";0x0;0x0";
			opt->disc_str = to_c_str(&disc_str);
		}
		cout << "disc_str: " << opt->disc_str << endl;
		break;
	case '4':
		if (!opt->do_adapt) {
			for (it = cfg->begin(); it != cfg->end(); it++) {
				if (it->dynmem && !it->dynmem->adp_addr)
					it->dynmem->adp_addr = it->dynmem->code_addr;
			}
		}
		if (strlen(opt->disc_str) == 1) {
			for (it = cfg->begin(); it != cfg->end(); it++) {
				if (it->dynmem && it->dynmem->adp_addr &&
				    !it->dynmem->adp_stack) {
					opt->disc_addr = it->dynmem->adp_addr;
					found = 1;
					break;
				}
			}
			if (!found)
				goto err;

			disc_str += opt->disc_str;
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
		} else {
			cerr << "Sorry, not supported, yet!" << endl;
			goto err;
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
