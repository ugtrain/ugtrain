/* cfgentry.h:     classes for config read from a file
 *
 * Copyright (c) 2012, by:      Sebastian Riemer
 *    All rights reserved.      Ernst-Reinke-Str. 23
 *                              10369 Berlin, Germany
 *                             <sebastian.riemer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 */

#ifndef CFGENTRY_H
#define CFGENTRY_H

#include <string>
#include <list>
using namespace std;

enum {
	DO_UNCHECKED,
	DO_LT,
	DO_GT
};

class CheckEntry {
public:
	long addr;
	bool is_signed;
	int size;
	long value;
	int check;
};

class CfgEntry {
public:
	string name;
	long addr;
	bool is_signed;
	int size;
	long value;
	long old_val;
	int check;
	list<CheckEntry> *checks;
};

#endif
