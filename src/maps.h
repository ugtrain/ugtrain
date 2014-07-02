/* maps.h:    parse /proc/$pid/maps
 *
 * Copyright (C) 2006,2007,2009 Tavis Ormandy <taviso@sdf.lonestar.org>
 * Copyright (C) 2009           Eli Dupree <elidupree@charter.net>
 * Copyright (C) 2009,2010      WANG Lu <coolwanglu@gmail.com>
 * Copyright (c) 2014           Sebastian Parschauer
 *    All rights reserved.     <s.parschauer@gmx.de>
 *
 * This file has been taken from scanmem.
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MAPS_H
#define MAPS_H

#include <list>
#include "common.h"

enum region_type {
	REGION_TYPE_MISC,
	REGION_TYPE_CODE,
	REGION_TYPE_EXE,
	REGION_TYPE_HEAP,
	REGION_TYPE_STACK
};

#define REGION_TYPE_NAMES { "misc", "code", "exe", "heap", "stack" }
extern const char *region_type_names[];

// a region obtained from /proc/pid/maps
struct region {
	ulong start;
	ulong size;
	enum region_type type;
	ulong load_addr;
	struct __attribute__((packed)) {
		u32 read:1;
		u32 write:1;
		u32 exec:1;
		u32 shared:1;
		u32 priv:1;
	} flags;
	u32 id;             // unique identifier
	string *filename;   // associated file
};

bool readmaps(pid_t pid, list<struct region> *regions);

static inline void list_regions (list<struct region> *rlist)
{
	list<struct region>::iterator region;

	list_for_each (rlist, region)
		printf("[%2u] %14lx, %7lu bytes, %5s, "
			"%14lx, %c%c%c, %s\n", region->id,
			(ulong) region->start, region->size,
			region_type_names[region->type], region->load_addr,
			region->flags.read ? 'r' : '-',
			region->flags.write ? 'w' : '-',
			region->flags.exec ? 'x' : '-',
			region->filename ? region->filename->c_str() : "unassociated");
}

static inline void read_regions (pid_t pid, list<struct region> *rlist)
{
	rlist->clear();
	if (!readmaps(pid, rlist))
		rlist->clear();
}

#endif
