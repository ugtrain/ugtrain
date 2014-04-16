/* cfgoutput.cpp:    output functions for the ugtrain config
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
 */

#include <cstring>

// local includes
#include "cfgoutput.h"


static void output_val (struct type *type, value_t value, const char *suffix)
{
	if (type->is_float) {
		if (type->size == 32)
			cout << value.f32 << suffix;
		else
			cout << value.f64 << suffix;
		goto out;
	}
	if (type->is_signed) {
		switch (type->size) {
		case 64:
			cout << value.i64 << suffix;
			break;
		case 32:
			cout << value.i32 << suffix;
			break;
		case 16:
			cout << (i32) value.i16 << suffix;
			break;
		default:
			cout << (i32) value.i8 << suffix;
			break;
		}
	} else {
		switch (type->size) {
		case 64:
			cout << value.u64 << suffix;
			break;
		case 32:
			cout << value.u32 << suffix;
			break;
		case 16:
			cout << (u32) value.u16 << suffix;
			break;
		default:
			cout << (u32) value.u8 << suffix;
			break;
		}
	}
out:
	return;
}

static void output_config_val (CfgEntry *cfg_en)
{
	switch (cfg_en->dynval) {
	case DYN_VAL_WATCH:
		cout << "(watch)" << endl;
		break;
	case DYN_VAL_MIN:
		output_val(&cfg_en->type, cfg_en->value, " (min)");
		cout << endl;
		break;
	case DYN_VAL_MAX:
		output_val(&cfg_en->type, cfg_en->value, " (max)");
		cout << endl;
		break;
	case DYN_VAL_ADDR:
		if (cfg_en->cfg_ref)
			cout << cfg_en->cfg_ref->name << endl;
		else
			cout << "0x" << hex << cfg_en->value.i64
				<< " (from addr)" << dec << endl;
		break;
	default:
		output_val(&cfg_en->type, cfg_en->value, "");
		cout << endl;
		break;
	}
}

static void output_config_en (CfgEntry *cfg_en)
{
	if (cfg_en->ptrtgt) {
		cout << "  " << cfg_en->name << " 0x" << hex << (long) cfg_en->addr << dec
			<< " " << 8 * sizeof(void *) << "-bit -> "
			<< cfg_en->ptrtgt->name << endl;
	} else {
		cout << "  " << cfg_en->name << " 0x" << hex << (long) cfg_en->addr << dec
			<< " " << cfg_en->type.size << "-bit ";
		output_config_val(cfg_en);
	}
}

static void output_ptrmem (CfgEntry *cfg_en)
{
	list<CfgEntry*> *cfg_act = &cfg_en->ptrtgt->cfg_act;
	list<CfgEntry*>::iterator it;

	list_for_each (cfg_act, it) {
		cfg_en = *it;
		cout << "  ->";
		output_config_en(cfg_en);
	}
}

static char *get_objcheck_str (CheckEntry *chk_en)
{
	return (char *) ((chk_en->is_objcheck) ? " (objcheck)" : "");
}

static char *get_check_op (check_e check)
{
	char *check_op;

	switch (check) {
	case CHECK_LT:
		check_op = (char *) " < ";
		break;
	case CHECK_GT:
		check_op = (char *) " > ";
		break;
	case CHECK_EQ:
		check_op = (char *) " == ";
		break;
	default:
		check_op = NULL;
		break;
	}
	return check_op;
}

static void output_checks (CfgEntry *cfg_en)
{
	list<CheckEntry> *chk_lp = cfg_en->checks;
	list<CheckEntry>::iterator it;
	u32 i;
	char *check_op = get_check_op(cfg_en->check);

	if (check_op) {
		cout << "    check 0x" << hex << (long) cfg_en->addr << dec << check_op;
		output_val(&cfg_en->type, cfg_en->value, "");
		cout << endl;
	}
	if (!chk_lp)
		return;

	list_for_each (chk_lp, it) {
		if (it->cfg_ref)
			cout << "    check " << it->cfg_ref->name;
		else
			cout << "    check 0x" << hex << (long) it->addr << dec;
		for (i = 0; it->check[i] != CHECK_END; i++) {
			if (i > 0)
				cout << " ||";
			check_op = get_check_op(it->check[i]);
			cout << check_op;
			output_val(&it->type, it->value[i], get_objcheck_str(&(*it)));
		}
		cout << endl;
	}
}

void output_configp (list<CfgEntry*> *cfg)
{
	if (!cfg || cfg->empty()) {
		cout << "<none>" << endl;
		return;
	}

	CfgEntry *cfg_en;

	list<CfgEntry*>::iterator it;
	list_for_each (cfg, it) {
		cfg_en = *it;
		output_config_en(cfg_en);
		if (cfg_en->ptrtgt)
			output_ptrmem (cfg_en);
	}
}

void output_config (list<CfgEntry> *cfg)
{
	if (!cfg || cfg->empty()) {
		cout << "<none>" << endl;
		return;
	}

	CfgEntry cfg_en;

	list<CfgEntry>::iterator it;
	list_for_each (cfg, it) {
		cfg_en = *it;
		// headline
		if (cfg_en.dynmem)
			cout << "dynmem: " << cfg_en.dynmem->name << " "
				<< cfg_en.dynmem->mem_size << " "
				<< hex << cfg_en.dynmem->code_addr << " "
				<< cfg_en.dynmem->stack_offs << dec << endl;
		else if (cfg_en.ptrmem)
			cout << "ptrmem: " << cfg_en.ptrmem->name << " "
				<<  cfg_en.ptrmem->mem_size << endl;
		else
			cout << "static: " << endl;

		// value line
		output_config_en(&cfg_en);
		output_checks(&cfg_en);
	}
}
