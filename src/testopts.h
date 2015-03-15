/* testopts.h:    additional command line options for testing
 *
 * Copyright (c) 2014..2015 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * powered by the Open Game Cheating Association
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef TESTOPTS_H
#define TESTOPTS_H

#include <string.h>

#ifdef TESTING

enum parts {
	PART_ALL,
	PART_OPT_PARSING,
	PART_CFG_PARSING,
	PART_CFG_OUTPUT
};

#define TESTING_OPT_VARS \
	enum parts	exit_after;

#define TESTING_OPT_HELP \
"--exit-after=<part>:	exit after the given part (for testing) - parts are:\n"\
"			\'optparsing\', \'cfgparsing\' and \'cfgoutput\'\n"

#define TESTING_OPT_CHARS \
	ExitAfter,

#define TESTING_OPT_LONG \
	{"exit-after",     1, 0, ExitAfter },

#define TESTING_OPT_PARSING						\
	case ExitAfter:							\
		if (strncmp(optarg, "optparsing", 10) == 0)		\
			opt->exit_after = PART_OPT_PARSING;		\
		else if (strncmp(optarg, "cfgparsing", 10) == 0)	\
			opt->exit_after = PART_CFG_PARSING;		\
		else if (strncmp(optarg, "cfgoutput", 9) == 0)		\
			opt->exit_after = PART_CFG_OUTPUT;		\
		break;

#else

#define TESTING_OPT_VARS
#define TESTING_OPT_HELP
#define TESTING_OPT_CHARS
#define TESTING_OPT_LONG
#define TESTING_OPT_PARSING

#endif

#endif
