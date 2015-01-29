/* maps.h:    parse /proc/$pid/maps
 *
 * Copyright (C) 2006,2007,2009 Tavis Ormandy <taviso@sdf.lonestar.org>
 * Copyright (C) 2009           Eli Dupree <elidupree@charter.net>
 * Copyright (C) 2009,2010      WANG Lu <coolwanglu@gmail.com>
 * Copyright (c) 2014..15       Sebastian Parschauer <s.parschauer@gmx.de>
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

#include <stdio.h>
#include <unistd.h>   /* readlink */

#ifdef __cplusplus
	#include <iostream>
	#include <list>
	#include <string>
	using namespace std;
#endif

/* local includes */
#include "types.h"

/* buffer size for reading symbolic links */
#define MAPS_MAX_PATH 256

/* a map obtained from /proc/pid/maps */
struct map {
	ulong start;
	ulong end;
	char read, write, exec, cow;
	i32 offset, dev_major, dev_minor, inode;
	char *file_path;
};

#ifdef __cplusplus
extern "C" {
#endif
	i32 read_maps  (pid_t pid, i32 (*callback)(struct map *map, void *data),
			void *data);
#ifdef __cplusplus
};
#endif


/* Assumption: sizeof(exe_path) >= MAPS_MAX_PATH */
static inline void get_exe_path_by_pid (pid_t pid, char exe_path[],
					size_t path_size)
{
	char link_path[128];
	ssize_t ret;

	/* get executable path */
	snprintf(link_path, sizeof(link_path), "/proc/%u/exe", pid);
	ret = readlink(link_path, exe_path, path_size);
	if (ret > 0) {
		exe_path[ret] = '\0';
	} else {
		/* readlink() may fail for special processes, treat as empty */
		exe_path[0] = '\0';
	}
}

#ifdef __cplusplus

enum region_type {
	REGION_TYPE_MISC,
	REGION_TYPE_CODE,
	REGION_TYPE_EXE,
	REGION_TYPE_HEAP,
	REGION_TYPE_STACK
};

#define REGION_TYPE_NAMES { "misc", "code", "exe", "heap", "stack" }
extern const char *region_type_names[];

/* a region obtained from /proc/pid/maps */
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
	u32 id;             /* unique identifier */
	string *file_path;  /* associated file path */
};

/* process_map() parameters */
struct pmap_params {
	char *exe_path;
	list<struct region> *rlist;
};

i32 process_map (struct map *map, void *data);

#ifndef list_for_each
#define list_for_each(list, it) \
	for (it = list->begin(); it != list->end(); ++it)
#endif

static inline void list_regions (list<struct region> *rlist)
{
	list<struct region>::iterator region;

	list_for_each (rlist, region)
		printf("[%2u] %14lx, %7lu bytes, %5s, "
			"%14lx, %c%c%c, %s\n", region->id,
			region->start, region->size,
			region_type_names[region->type], region->load_addr,
			region->flags.read ? 'r' : '-',
			region->flags.write ? 'w' : '-',
			region->flags.exec ? 'x' : '-',
			region->file_path ? region->file_path->c_str() : "unassociated");
}

static inline void read_regions (pid_t pid, struct pmap_params *params)
{
	list<struct region> *rlist = params->rlist;

	rlist->clear();
	if (read_maps(pid, process_map, params))
		rlist->clear();
}
#endif

#endif
