/* adaption.cpp:    adapt the config by running a script
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

#include <cstring>
#include <fstream>
#include <stdio.h>

// local includes
#include <lib/getch.h>
#include <lib/system.h>
#include <adaption.h>
#include <common.h>
#include <commont.cpp>


static i32 write_config_vect (char *path, vector<string> *lines)
{
	ofstream cfg_file;
	vector<string>::iterator it;

	lines->pop_back();

	cfg_file.open(path, fstream::trunc);
	if (!cfg_file.is_open()) {
		cerr << "File \"" << path << "\" doesn't exist!" << endl;
		return -1;
	}
	vect_for_each (lines, it)
		cfg_file << (*it) << endl;

	cfg_file.close();
	return 0;
}

i32 take_over_config (struct app_options *opt, list<CfgEntry> *cfg,
		       vector<string> *lines)
{
	list<CfgEntry>::iterator cfg_it;
	DynMemEntry *tmp = NULL;
	u32 lnr;
	i32 ret = 0;

	list_for_each (cfg, cfg_it) {
		if (!cfg_it->dynmem || cfg_it->dynmem == tmp)
			continue;
		tmp = cfg_it->dynmem;
		tmp->mem_size = tmp->adp_size;
		tmp->code_addr = tmp->adp_addr;
		if (tmp->adp_soffs)
			tmp->stack_offs = tmp->adp_soffs;
		lnr = tmp->cfg_line;
		lines->at(lnr) = "dynmemstart " + tmp->name + " "
			+ to_string(tmp->mem_size) + " 0x"
			+ to_xstring(tmp->code_addr) + " 0x"
			+ to_xstring(tmp->stack_offs);
	}
	// Adaption isn't required anymore
	lnr = opt->adp_req_line;
	if (lnr > 0)
		lines->at(lnr) = "adapt_required 0";

	// Write back config
	cout << "Writing back config.." << endl;
	ret = write_config_vect(opt->cfg_path, lines);
	if (ret)
		return ret;

	// Run game with libmemhack
	opt->do_adapt = false;
	opt->disc_str = NULL;
	use_libmemhack(opt);

	return ret;
}

static i32 parse_adapt_result (struct app_options *opt, list<CfgEntry> *cfg,
			       char *buf, ssize_t buf_len)
{
	char *part_end = buf;
	ssize_t part_size, ppos = 0;
	u32 i, num_obj = 0;
	string *obj_name = NULL;
	u32 malloc_size = 0;
	ptr_t code_addr = 0;
	list<CfgEntry>::iterator it;
	DynMemEntry *tmp = NULL;
	bool found;

	part_end = strchr(buf, ';');
	if (part_end == NULL)
		goto parse_err;
	if (sscanf(buf, "%u", &num_obj) != 1)
		goto parse_err;
	part_size = part_end - buf;
	ppos += part_size + 1;

	for (i = 1; i <= num_obj; i++) {
		part_end = strchr(buf + ppos, ';');
		if (part_end == NULL)
			goto parse_err;
		part_size = part_end - (buf + ppos);
		obj_name = new string(buf + ppos, part_size);
		ppos += part_size + 1;

		if (sscanf(buf + ppos, "%x", &malloc_size) != 1)
			goto parse_err;
		part_end = strchr(buf + ppos, ';');
		if (part_end == NULL)
			goto parse_err;
		part_size = part_end - (buf + ppos);
		ppos += part_size + 1;

		if (sscanf(buf + ppos, SCN_PTR, &code_addr) != 1)
			goto parse_err;

		// find object and set adp_size and adp_addr
		found = false;
		list_for_each (cfg, it) {
			tmp = it->dynmem;
			if (tmp && !tmp->adp_addr &&
			    tmp->name == *obj_name) {
				tmp->adp_size = malloc_size;
				tmp->adp_addr = code_addr;
				cout << "Class " << tmp->name
				     << ", old_size: " << tmp->mem_size
				     << ", new_size: " << tmp->adp_size
				     << endl;
				cout << "Class " << tmp->name
				     << ", old_code: 0x" << hex << tmp->code_addr
				     << ", new_code: 0x" << tmp->adp_addr
				     << dec << endl;
				found = true;
				break;
			}
		}
		if (!found)
			goto parse_err;
		if (i == num_obj)
			break;

		part_end = strchr(buf + ppos, ';');
		if (part_end == NULL)
			goto parse_err;
		part_size = part_end - (buf + ppos);
		ppos += part_size + 1;
	}

	return 0;
parse_err:
	cerr << "Error while parsing adaption output!" << endl;
	if (buf[buf_len - 1] == '\n')
		cerr << "-->" << (buf + ppos);
	else
		cerr << "-->" << (buf + ppos) << endl;
	return -1;
}

static i32 adapt_config (struct app_options *opt, list<CfgEntry> *cfg)
{
	char pbuf[PIPE_BUF] = { 0 };
	ssize_t read_bytes;
	const char *cmd = (const char *) opt->adp_script;
	char *cmdv[] = {
		opt->adp_script,
		opt->game_binpath,
		NULL
	};

#ifdef __linux__
	if (getuid() == 0)
		goto err;
#endif

	read_bytes = run_cmd_pipe(cmd, cmdv, pbuf, sizeof(pbuf));
	if (read_bytes <= 0)
		goto err;
	cout << "Adaption return:" << endl;
	if (pbuf[read_bytes - 1] == '\n')
		cout << pbuf;
	else
		cout << pbuf << endl;

	if (parse_adapt_result(opt, cfg, pbuf, read_bytes) != 0)
		goto err;

	return 0;
err:
	cerr << "Error while running adaption script!" << endl;
	return -1;
}

i32 process_adaption (struct app_options *opt, list<CfgEntry> *cfg,
		      vector<string> *lines)
{
	char ch;
	i32 ret = 0;

	if (opt->adp_required && !opt->do_adapt && !opt->disc_str &&
	    !opt->run_scanmem) {
		if (!opt->adp_script) {
			cerr << "Error, adaption required but no adaption script!" << endl;
			ret = -1;
			goto out;
		}
		cout << "Adaption to your compiler/game version is required." << endl;
		cout << "Adaption script: " << opt->adp_script << endl;
		cout << "Run the adaption script, now (y/n)? : ";
		fflush(stdout);
		ch = 'n';
		ch = do_getch();
		cout << ch << endl;
		if (ch == 'y') {
			opt->do_adapt = true;
			do_assumptions(opt);
		}
	}

	if (opt->do_adapt) {
		if (!opt->adp_script) {
			cerr << "Error, no adaption script!" << endl;
			ret = -1;
			goto out;
		}
		ret = adapt_config(opt, cfg);
		if (ret) {
			cerr << "Error while size or code address adaption!"
			     << endl;
			goto out;
		}
		if (opt->use_gbt) {
			ret = take_over_config(opt, cfg, lines);
			if (ret)
				goto out;
		} else {
			cout << "Adapt reverse stack offset(s) (y/n)? : ";
			fflush(stdout);
			ch = 'n';
			ch = do_getch();
			cout << ch << endl;
			if (ch != 'y') {
				ret = take_over_config(opt, cfg, lines);
				if (ret)
					goto out;
			}
		}
	}
out:
	return ret;
}
