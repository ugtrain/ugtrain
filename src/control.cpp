/* control.cpp:    change processing according to user input
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

// local includes
#include <control.h>
#include <dump.h>


typedef enum {
	INCREMENT,
	DECREMENT,
} chgmode_e;

static void change_mvec_pridx (list<CfgEntry> *cfg, chgmode_e mode)
{
	DynMemEntry *old_dynmem = NULL;
	list<CfgEntry>::iterator it;

	list_for_each (cfg, it) {
		if (it->dynmem && it->dynmem != old_dynmem) {
			if (mode == INCREMENT)
				it->dynmem->pr_idx++;
			else if (it->dynmem->pr_idx > 0)
				it->dynmem->pr_idx--;
			old_dynmem = it->dynmem;
		}
	}
}

static void toggle_cfg (list<CfgEntry*> *key_cfg, list<CfgEntry*> *cfg_act)
{
	bool found;
	CfgEntry *cfg_en;
	list<CfgEntry*> *used_cfg_act = NULL;

	list<CfgEntry*>::iterator it, it_act;
	list_for_each (key_cfg, it) {
		cfg_en = *it;
		if (cfg_en->ptrmem)
			used_cfg_act = &cfg_en->ptrmem->cfg_act;
		else
			used_cfg_act = cfg_act;
		found = false;
		list_for_each (used_cfg_act, it_act) {
			if (cfg_en == *it_act) {
				used_cfg_act->erase(it_act);
				cfg_en = *it_act;
				cout << cfg_en->name << " OFF" << endl;
				found = true;
				break;
			}
		}
		if (!found) {
			used_cfg_act->push_back(cfg_en);
			cout << cfg_en->name << " ON" << endl;
		}
	}
}

/*
 * handles input char by performing related action
 *
 * ATTENTION: ch has to be checked before calling this function!
 *            It must fit into cfgp_map!
 */
void handle_input_char (char ch, list<CfgEntry*> *cfgp_map[], pid_t pid,
			list<CfgEntry> *cfg, list<CfgEntry*> *cfg_act)
{
	i32 ch_idx = (i32) ch;

	switch (ch) {
	case '>':
		dump_all_mem_obj(pid, cfg);
		break;
	case '+':
		change_mvec_pridx(cfg, INCREMENT);
		break;
	case '-':
		change_mvec_pridx(cfg, DECREMENT);
		break;
	default:
		if (cfgp_map[ch_idx])
			toggle_cfg(cfgp_map[ch_idx], cfg_act);
		break;
	}
}
