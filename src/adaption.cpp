/* adaption.cpp:    adapt the config by running a script
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

#include <cstring>
#include <fstream>
#include <stdio.h>
#include <libgen.h>   // basename

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
		ugerr << "File \"" << path << "\" doesn't exist!" << endl;
		return -1;
	}
	vect_for_each (lines, it)
		cfg_file << (*it) << endl;

	cfg_file.close();
	return 0;
}

static i32 take_over_config (Options *opt, list<CfgEntry> *cfg,
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
	ugout << "Writing back config.." << endl;
	ret = write_config_vect(opt->cfg_path, lines);
	if (ret)
		return ret;

	// Run game with libmemhack
	opt->do_adapt = false;
	opt->disc_str = NULL;
	use_libmemhack(opt);

	return ret;
}

static inline i32 parse_adp_string (char **start, char **obj_name)
{
	char *end;
	size_t size;

	end = strchr(*start, ';');
	if (end == NULL)
		return 1;
	*end = '\0';
	size = end - *start;
	*obj_name = *start;
	*start += size + 1;
	return 0;
}

static i32 parse_adapt_result (Options *opt, list<CfgEntry> *cfg,
			       char *buf, ssize_t buf_len,
			       vector<string> *lines)
{
	char *end = buf, *start = buf;
	ssize_t size;
	i32 lnr = -1;		// -1: object name, >= 0: reserved cfg entry
	u32 i, num_obj = 0;
	char *obj_name = NULL;
	u32 malloc_size = 0;
	ptr_t code_addr = 0;
	list<CfgEntry>::iterator it;
	DynMemEntry *tmp = NULL;
	bool found;

	end = strchr(start, ';');
	if (end == NULL)
		goto parse_err;
	if (sscanf(start, "%u", &num_obj) != 1)
		goto parse_err;
	size = end - start;
	start += size + 1;

	for (i = 1; i <= num_obj; i++) {
		// get object name or reserved cfg entry
		for (;;) {
			if (parse_adp_string(&start, &obj_name))
				goto parse_err;
			if (!strncmp(obj_name, "proc_name",
				     sizeof("proc_name") - 1)) {
				lnr = 0;
			} else if (!strncmp(obj_name, "game_binpath",
					    sizeof("game_binpath") - 1)) {
				lnr = opt->binpath_line;
				// Not configured?
				if (lnr <= 0) {
					// ignore the game binpath argument
					if (parse_adp_string(&start, &obj_name))
						goto parse_err;
					lnr = -1;
					continue;
				}
			}
			// Reserved cfg entry read?
			if (lnr >= 0) {
				string tmp_line;
				// read parameter
				if (parse_adp_string(&start, &obj_name))
					goto parse_err;
				if (lnr == 0) {
					tmp_line = string(obj_name);
					if (strcmp(opt->game_call, opt->proc_name) == 0) {
						opt->proc_name = to_c_str(&tmp_line);
						opt->game_call = opt->proc_name;
					} else {
						opt->proc_name = to_c_str(&tmp_line);
					}
				} else if ((u32) lnr == opt->binpath_line) {
					tmp_line = string(obj_name);
					opt->game_binpath = to_c_str(&tmp_line);
					if (strcmp(basename(opt->game_binpath),
					    opt->proc_name) != 0)
						goto parse_err;
					tmp_line = "game_binpath " + string(obj_name);
				}
				lines->at(lnr) = tmp_line;
				lnr = -1;
				continue;
			}
			break;
		}
		if (sscanf(start, "%x", &malloc_size) != 1)
			goto parse_err;
		end = strchr(start, ';');
		if (end == NULL)
			goto parse_err;
		size = end - start;
		start += size + 1;

		if (sscanf(start, SCN_PTR, &code_addr) != 1)
			goto parse_err;

		// find object and set adp_size and adp_addr
		found = false;
		list_for_each (cfg, it) {
			tmp = it->dynmem;
			if (tmp && !tmp->adp_addr &&
			    tmp->name == string(obj_name)) {
				tmp->adp_size = malloc_size;
				tmp->adp_addr = code_addr;
				ugout << "Class " << tmp->name
				      << ", old_size: " << tmp->mem_size
				      << ", new_size: " << tmp->adp_size
				      << endl;
				ugout << "Class " << tmp->name
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

		end = strchr(start, ';');
		if (end == NULL)
			goto parse_err;
		size = end - start;
		start += size + 1;
	}

	return 0;
parse_err:
	ugerr << "Error while parsing adaptation output!" << endl;
	if (buf[buf_len - 1] == '\n')
		ugerr << "-->" << start;
	else
		ugerr << "-->" << start << endl;
	return -1;
}

static i32 adapt_config (Options *opt, list<CfgEntry> *cfg,
			 vector<string> *lines)
{
	bool debug_mode = false;
	char pbuf[PIPE_BUF] = { 0 };
	ssize_t read_bytes = 0;
	const char *cmd = (const char *) opt->adp_script;
	char *cmdv[] = {
		opt->adp_script,
		opt->game_binpath,
		NULL, NULL
	};

#ifdef __linux__
	if (getuid() == 0)
		goto err;
#endif

start:
	if (!debug_mode)
		read_bytes = run_cmd_pipe(cmd, cmdv, pbuf, sizeof(pbuf));
	else
		run_cmd(cmd, cmdv);
	if (read_bytes <= 0)
		goto err;
	ugout << "Adaptation return:" << endl;
	if (pbuf[read_bytes - 1] == '\n')
		cout << pbuf;
	else
		cout << pbuf << endl;

	if (parse_adapt_result(opt, cfg, pbuf, read_bytes, lines) != 0)
		goto err;

	return 0;
err:
	if (!debug_mode) {
		debug_mode = true;
		cmdv[2] = (char *) "DEBUG";
		goto start;
	}
	ugerr << "Error while running adaptation script!" << endl;
	return -1;
}

i32 process_adaption (Options *opt, list<CfgEntry> *cfg,
		      vector<string> *lines)
{
	char ch;
	i32 ret = 0;

	if (opt->adp_required && !opt->do_adapt && !opt->disc_str &&
	    !opt->run_scanmem) {
		if (!opt->adp_script) {
			ugerr << "Error, adaptation required but no adaptation script!" << endl;
			ret = -1;
			goto out;
		}
		ugout << "Adaptation to your compiler/game version is required." << endl;
		ugout << "Adaptation script: " << opt->adp_script << endl;
		ugout << "Run the adaptation script, now (y/n)? : ";
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
			ugerr << "Error, no adaptation script!" << endl;
			ret = -1;
			goto out;
		}
		ret = adapt_config(opt, cfg, lines);
		if (ret) {
			ugerr << "Error while size or code address adaptation!"
			      << endl;
			goto out;
		}
		ret = take_over_config(opt, cfg, lines);
		if (ret)
			goto out;
	}
out:
	return ret;
}
