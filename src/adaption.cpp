/* adaption.cpp:    adapt the config by running a script
 *
 * Copyright (c) 2012..13, by:  Sebastian Riemer
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

#include <cstring>
#include <stdio.h>

// local includes
#include "adaption.h"
#include "system.h"


static i32 parse_adapt_result (struct app_options *opt, list<CfgEntry> *cfg,
			       char *buf, ssize_t buf_len)
{
	char *part_end = buf;
	ssize_t part_size, ppos = 0;
	u32 i, num_obj = 0;
	string *obj_name = NULL;
	void *code_addr = NULL;
	list<CfgEntry>::iterator it;
	DynMemEntry *tmp = NULL;
	int found;

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

		if (sscanf(buf + ppos, "%p", &code_addr) != 1)
			goto parse_err;

		// find object and set adp_addr
		found = 0;
		for (it = cfg->begin(); it != cfg->end(); it++) {
			tmp = it->dynmem;
			if (tmp && !tmp->adp_addr &&
			    tmp->name == *obj_name) {
				tmp->adp_addr = code_addr;
				cout << "Class " << tmp->name
				     << ", old_code: " << hex << tmp->code_addr
				     << ", new_code: " << tmp->adp_addr
				     << dec << endl;
				found = 1;
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

i32 adapt_config (struct app_options *opt, list<CfgEntry> *cfg)
{
	char pbuf[PIPE_BUF] = { 0 };
	ssize_t read_bytes;
	const char *cmd = (const char *) opt->adp_script;
	char *cmdv[] = {
		opt->adp_script,
		opt->game_path,
		NULL
	};

#ifdef __linux__
	if (getuid() == 0)
		goto err;
#endif

	read_bytes = run_cmd_pipe(cmd, cmdv, pbuf, sizeof(pbuf), false);
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
