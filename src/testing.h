/* testing.h:    inline functions to check if ugtrain does it right
 *
 * Copyright (c) 2014..2015 Sebastian Parschauer <s.parschauer@gmx.de>
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

#ifndef TESTING_H
#define TESTING_H

#include <list>
#include <stdlib.h>

/* local includes */
#include <cfgentry.h>
#include <options.h>
#include <testopts.h>

#ifdef TESTING

static inline void test_optparsing (struct app_options *opt)
{
	if (opt->exit_after != PART_OPT_PARSING)
		return;
	exit(0);
}

static inline void test_cfgparsing (struct app_options *opt,
				    list<CfgEntry> *cfg,
				    list<CfgEntry*> *cfg_act,
				    list<CfgEntry*> *cfgp_map[])
{
	if (opt->exit_after != PART_CFG_PARSING)
		return;
	exit(0);
}

static inline void test_cfgoutput (struct app_options *opt)
{
	if (opt->exit_after != PART_CFG_OUTPUT)
		return;
	exit(0);
}

#else

static inline void test_optparsing (struct app_options *opt) {}
static inline void test_cfgparsing (struct app_options *opt,
				    list<CfgEntry> *cfg,
				    list<CfgEntry*> *cfg_act,
				    list<CfgEntry*> *cfgp_map[]) {}
static inline void test_cfgoutput (struct app_options *opt) {}

#endif

#endif
