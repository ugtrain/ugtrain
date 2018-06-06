/* common.cpp:    common C++ helpers
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
 */

#include <cstdlib>
#include <cstring>

// local includes
#include <common.h>


char *to_c_str (string *str)
{
	char *cstr = new char[str->size() + 1];

	cstr[str->size()] = '\0';
	memcpy(cstr, str->c_str(), str->size());
	return cstr;
}

char *to_c_str_c (string *str)
{
	char *cstr = (char *) malloc(str->size() + 1);

	cstr[str->size()] = '\0';
	memcpy(cstr, str->c_str(), str->size());
	return cstr;
}
