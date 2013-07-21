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

#include <cstring>
#include "common.h"
#include "common.cpp"
#include "discovery.h"

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
			opt->disc_str = new char[disc_str.size() + 1];
			opt->disc_str[disc_str.size()] = '\0';
			memcpy(opt->disc_str, disc_str.c_str(),
				disc_str.size());
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
