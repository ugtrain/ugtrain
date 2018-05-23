/* discovery.cpp:    discover dynamic memory objects
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
#include <aslr.h>
#include <cfgparser.h>
#include <common.h>
#include <commont.cpp>
#include <discovery.h>
#include <fifoparser.h>
#include <util.h>

#define DASM_DIR "/tmp/"
#define DASM_SUF "_dasm"
#define MAX_BT 11
#define MAX_MATCHES 15
#define DISC_DEBUG 0


// read a text file with a disassembly included and
// search for a library function call in front of the code address
static inline ssize_t code_addr_to_func_name (ptr_t code_addr,
					      string *dasm_fpath,
					      char *pbuf,
					      size_t buf_size)
{
	string cmd_str;
	ssize_t rbytes;

	cmd_str = "cat ";
	cmd_str += *dasm_fpath;
	cmd_str += " | grep -B 1 -e \"^[ ]\\+";
	cmd_str += to_xstring(code_addr);
	cmd_str += ":\" | head -n 1 | grep -o -e \"call.*$\" "
		   "| grep -o -e \"<.*@plt>\"";
#if (DISC_DEBUG)
	cout << "$ " << cmd_str << endl;
#endif
	rbytes = run_cmd_pipe(cmd_str.c_str(), NULL, pbuf, buf_size);
	if (rbytes > 0) {
		// remove trailing new line
		if (pbuf[rbytes - 1] == '\n')
			pbuf[rbytes - 1] = '\0';
	}
	return rbytes;
}

// disassemble a binary file and store the text output in another file
static inline i32 disassemble_file (string *ipath, string *opath)
{
	string cmd_str;
	i32 ret;

	cmd_str = "objdump -D ";
	cmd_str += *ipath;
	cmd_str += " > ";
	cmd_str += *opath;
#if (DISC_DEBUG)
	cout << "$ " << cmd_str << endl;
#endif
	ret = run_cmd(cmd_str.c_str(), NULL);
	if (ret > 0)
		ret = 0;
	return ret;
}

// get the region name, its file path and subtract the load address
static inline i32 code_addr_to_region (ptr_t *code_addr,
				       struct list_head *rlist,
				       string *bin_path,
				       string *bin_name,
				       ptr_t exe_offs)
{
	struct region *it;
	i32 ret = -1;

	clist_for_each_entry (it, rlist, list) {
		char *fpath = it->file_path;
		size_t pos;

		if (!it->flags.exec || it->start + it->size < *code_addr ||
		    !fpath || fpath[0] != '/')
			continue;
		if (it->start > *code_addr)
			break;
		*bin_path = fpath;
		pos = bin_path->find_last_of('/');
		*bin_name = bin_path->substr(pos + 1);
		if (it->type == REGION_TYPE_EXE)
			*code_addr -= exe_offs;
		else
			*code_addr -= it->load_addr;
		ret = 0;
		break;
	}
	return ret;
}

// post parsing parameters for process_disc1234_malloc()
struct disc_pp {
	ptr_t in_addr;
	Options *opt;
	struct list_head *rlist;
};

// mf() callback for read_dynmem_buf()
static void process_disc1234_malloc (MF_PARAMS)
{
	struct disc_pp *dpp = (struct disc_pp *) pp->argp;
	ptr_t in_addr = dpp->in_addr;
	Options *opt = dpp->opt;
	struct list_head *rlist = dpp->rlist;
	char stage = opt->disc_str[0];
	ptr_t codes[MAX_BT] = { 0 };
	ptr_t soffs[MAX_BT] = { 0 };
	char *sep_pos;
	i32 i, ret, num_codes = 0;
	static u32 matches_left = MAX_MATCHES;

	if (in_addr >= mem_addr &&
	    in_addr < mem_addr + mem_size) {
		// limit the number of printed matches
		if (matches_left == 0)
			return;
		matches_left--;

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
		for (i = 0; i < num_codes; i++) {
			string bin_path, bin_name;
			char pbuf[PIPE_BUF] = { 0 };
			ssize_t rbytes = 0;
			// get the region name and subtract the load address
			ret = code_addr_to_region(&codes[i], rlist,
				&bin_path, &bin_name, opt->code_offs);
			if (ret == 0 && opt->have_objdump) {
				// get the function call from disassembly
				string dasm_fpath;
				dasm_fpath = DASM_DIR;
				dasm_fpath += bin_name;
				dasm_fpath += DASM_SUF;
				// get disassembly only once
				if (!file_exists(dasm_fpath.c_str()))
					disassemble_file(&bin_path, &dasm_fpath);
				rbytes = code_addr_to_func_name(codes[i],
					&dasm_fpath, pbuf, sizeof(pbuf));
			}
			// output one call from backtrace
			if (!bin_name.empty()) {
				cout << "c0x" << hex << codes[i];
				if (stage == '4')
					cout << ";o0x" << hex << soffs[i]
					     << dec;
				cout << " " << bin_name << " " << pbuf << endl;
			}
			// cleanup pipe buffer
			if (rbytes > 0)
				memset(pbuf, 0, rbytes);
		}
	}
}

// sf() callback for read_dynmem_buf()
static void discover_stack_val (SF_PARAMS)
{
	struct disc_pp *dpp = (struct disc_pp *) argp;
	Options *opt = dpp->opt;
	ptr_t in_addr = dpp->in_addr;
	ptr_t stack_start = opt->stack_start;

	opt->stack_end = stack_end;
	if (!stack_start || !stack_end || stack_start > stack_end)
		return;
	if (in_addr >= stack_start && in_addr < stack_end)
		cout << "stack contains 0x" << hex << in_addr
		     << ", offs: 0x" << stack_end - in_addr
		     << dec << endl;
}

static inline void rm_dasm_files (void)
{
	rm_files_by_pattern((char *) DASM_DIR "*" DASM_SUF);
}

static i32 postproc_stage1234 (Options *opt, list<CfgEntry> *cfg,
			       struct list_head *rlist)
{
	i32 ifd, pmask = PARSE_S;
	ptr_t mem_addr;
	struct disc_pp dpp;
	struct parse_cb pcb = { NULL };

	ifd = open(opt->dynmem_file, O_RDONLY);
	if (ifd < 0) {
		perror("open ifd");
		goto err;
	}

	restore_getch();

	cout << "Memory address (e.g. 0xdeadbeef): ";
	fflush(stdout);
	cin >> hex >> mem_addr;
	if (!mem_addr) {
		cerr << "Error: Invalid memory address!" << endl;
		goto err_close;
	}
	cout << hex << "Searching reverse for 0x" << mem_addr << dec
	     << " in discovery output.." << endl;

	dpp.in_addr = mem_addr;
	dpp.opt = opt;
	dpp.rlist = rlist;

	pcb.mf = process_disc1234_malloc;
	pcb.sf = discover_stack_val;

	rm_dasm_files();
	while (true) {
		if (read_dynmem_buf(cfg, &dpp, ifd, pmask, true,
		    opt->code_offs, &pcb) < 0)
			break;
	}
	close(ifd);
	rm_dasm_files();

	if (prepare_getch() != 0) {
		cerr << "Error while terminal preparation!" << endl;
		goto err;
	}

	return 0;
err_close:
	close(ifd);
err:
	return -1;
}

i32 postproc_discovery (Options *opt, list<CfgEntry> *cfg,
			struct list_head *rlist, vector<string> *lines)
{
	if (opt->disc_str[0] >= '1' && opt->disc_str[0] <= '4')
		return postproc_stage1234(opt, cfg, rlist);
	return -1;
}

// parameter for run_stage1234_loop()
struct disc_params {
	i32 ifd;
	i32 pid;
	Options *opt;
};

/*
 * worker process
 * changed values aren't available in the parent
 */
static void run_stage1234_loop (void *argp)
{
	struct disc_params *params = (struct disc_params *) argp;
	i32 ifd = params->ifd;
	i32 pid = params->pid;
	Options *opt = params->opt;
	i32 ofd;
	char buf[PIPE_BUF] = { 0 };
	ssize_t rbytes, wbytes;
	enum pstate pstate;

	ofd = open(opt->dynmem_file, O_WRONLY | O_CREAT | O_TRUNC,
		   0644);

	if (ofd < 0) {
		perror("open ofd");
		exit(1);
	}

	while (true) {
		sleep_sec_unless_input(1, ifd);
		rbytes = read(ifd, buf, sizeof(buf) - 1);
		if (rbytes == 0 || (rbytes < 0 && errno == EAGAIN)) {
			pstate = check_process(pid, NULL);
			if (pstate == PROC_DEAD || pstate == PROC_ZOMBIE)
				sleep_sec(1);
			continue;
		}
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
	close(ofd);
	exit(1);
}

void process_discovery (Options *opt, list<CfgEntry> *cfg,
			i32 ifd, i32 dfd, i32 ofd, i32 pid,
			struct list_head *rlist)
{
	pid_t worker_pid;

	if (opt->disc_str[0] == 'p')
		opt->disc_str[0] = opt->disc_str[opt->disc_offs];
	if (opt->disc_str[0] == '0') {
		if (opt->scanmem_pid > 0) {
			wait_orphan(pid, opt->proc_name);
			wait_proc(opt->scanmem_pid);
			reset_sigint();
		} else {
			wait_orphan(pid, opt->proc_name);
		}
		exit(0);
	} else if (opt->disc_str[0] >= '1' && opt->disc_str[0] <= '4') {
		struct disc_params params = { dfd, pid, opt };
		get_stack_start(opt, pid, rlist);
		if (opt->disc_str[0] >= '3' || opt->disc_offs > 0)
			handle_aslr(opt, cfg, ifd, ofd, pid, rlist);
		worker_pid = fork_proc(run_stage1234_loop, &params);
		if (opt->disc_str[0] >= '3' && opt->disc_lib &&
		    opt->disc_lib[0] != '\0')
			do_disc_pic_work(pid, opt, ifd, ofd, rlist);
		else
			wait_orphan(pid, opt->proc_name);
		if (opt->scanmem_pid > 0) {
			wait_proc(opt->scanmem_pid);
			reset_sigint();
		}
		sleep_msec(250);  // chance to read the final flush
		kill_proc(worker_pid);
		if (worker_pid < 0)
			exit(-1);
	}
}

static inline void stage34_args_err (char stage)
{
	cerr << "Error: Not enough valid arguments for discovery stage "
	     << stage << "!" << endl;
	cerr << "Use at least \'" << stage << ";<size>\'"
	     << endl;
}

// Does the user request to filter the backtrace to the given lib?
// returns 0 for success, -1 for error
static inline i32 parse_bt_filter (char **disc_part, char **disc_lib,
				   char stage)
{
	i32 ret = -1;
	char *pos;

	if ((*disc_part)[0] != ';' || !isalpha((*disc_part)[1]))
		goto success;

	(*disc_part)++;
	pos = strchr(*disc_part, ';');
	if (!pos) {
		stage34_args_err(stage);
		goto out;
	}
	*pos = '\0';
#if (DISC_DEBUG)
	cout << "PIC: filtering backtrace to " << *disc_part << endl;
#endif
	if (strncmp(*disc_part, "exe", sizeof("exe") - 1) == 0) {
		*disc_lib = (char *) "\0";
	} else {
		*disc_lib = new char[strlen(*disc_part) + 1];
		strcpy(*disc_lib, *disc_part);
	}
	*pos = ';';
	*disc_part = pos;
success:
	ret = 0;
out:
	return ret;
}

i32 prepare_discovery (Options *opt, list<CfgEntry> *cfg)
{
	string disc_str, cmd_str;
	char *pos, *disc_part;
	char stage;
	i32 ret;
	list<CfgEntry>::iterator it;
	ptr_t code_addr = 0;
	ulong mem_size = 0;

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
			disc_str += ";0";
			opt->disc_str = to_c_str(&disc_str);
		} else {
			ret = sscanf(disc_part, ";%lu", &mem_size);
			if (ret < 0) {
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
		if (ret == 0)
			disc_part += sizeof(GBT_CMD);
		// fall through
	case '4':
		disc_part++;
		if (parse_bt_filter(&disc_part, &opt->disc_lib, stage))
			goto err;
		ret = sscanf(disc_part, ";%lu;" SCN_PTR, &mem_size, &code_addr);
		if (ret < 1) {
			stage34_args_err(stage);
			goto err;
		}
		opt->code_addr = code_addr;
		if (mem_size == 0) {
			cerr << "Error: Too much data! Please discover the "
				"size first!" << endl;
			goto err;
		}
		cout << "disc_str: " << opt->disc_str << endl;
		break;
	default:
		goto err;
	}

	return 0;
err:
	cerr << "Error while preparing discovery!" << endl;
	return -1;
}

static inline i32
check_beginner_stage4 (Options *opt)
{
	i32 ret;
	ulong mem_size;

	ret = sscanf(opt->disc_str, "%lu", &mem_size);
	if (ret == 1 && mem_size >= 8) {
		u32 i;
		string disc_str;
		for (i = 0; i < strlen(opt->disc_str); i++) {
			if (!isdigit(opt->disc_str[i]))
				goto err;
		}
		disc_str = "4;";
		disc_str += opt->disc_str;
		opt->disc_str = to_c_str(&disc_str);
	}
	return 0;
err:
	cerr << "Invalid discovery string.";
	return -1;
}

bool init_discovery (Options *opt, list<CfgEntry> *cfg,
		     list<CfgEntry*> *cfg_act)
{
	bool allow_empty_cfg = false;

	if (check_beginner_stage4(opt) != 0)
		exit(-1);
	if (opt->disc_str[0] >= '0' && opt->disc_str[0] <= '4') {
		if (opt->run_scanmem && !tool_is_available((char *) "scanmem"))
			exit(-1);
		if (opt->disc_str[0] >= '3') {
			opt->have_objdump = tool_is_available(
				(char *) "objdump");
			if (!opt->have_objdump)
				cerr << "Backtrace helpers and symbol "
				    "lookup aren't available." << endl;
		}
		cout << "Clearing config for discovery!" << endl;
		cfg->clear();
		cfg_act->clear();
		allow_empty_cfg = true;
	} else {
		opt->run_scanmem = false;
	}
	return allow_empty_cfg;
}

bool init_scanmem (Options *opt, list<CfgEntry> *cfg,
		   list<CfgEntry*> *cfg_act)
{
	bool allow_empty_cfg = true;

	if (!tool_is_available((char *) "scanmem"))
		exit(-1);
	cout << "Clearing config for scanmem!" << endl;
	cfg->clear();
	cfg_act->clear();

	return allow_empty_cfg;
}
