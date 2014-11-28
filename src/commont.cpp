/* commont.cpp:    common C++ template helpers
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

#include <sstream>

// local includes
#include <common.h>


/* cpp file needs to be included */
template <class T>
string to_string (T val)
{
	ostringstream ss;

	ss << val;
	return ss.str();
}

template <class T>
string to_xstring (T val)
{
	ostringstream ss;

	ss << hex << val << dec;
	return ss.str();
}
