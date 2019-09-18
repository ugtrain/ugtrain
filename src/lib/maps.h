/* maps.h:    parse /proc/$pid/maps
 *
 * Copyright (C) 2006,2007,2009 Tavis Ormandy <taviso@sdf.lonestar.org>
 * Copyright (C) 2009           Eli Dupree <elidupree@charter.net>
 * Copyright (C) 2009,2010      WANG Lu <coolwanglu@gmail.com>
 * Copyright (C) 2014..2015     Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file has been taken from scanmem.
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MAPS_H
#define MAPS_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>   /* readlink */

/* local includes */
#include "list.h"
#include "types.h"

/* buffer size for reading symbolic links */
#define MAPS_MAX_PATH 256

/* a map obtained from /proc/$pid/maps */
struct region_map {
	ulong start;
	ulong end;
	char read, write, exec, cow;
	i32 offset, dev_major, dev_minor, inode;
	char *file_path;
};

#ifdef __cplusplus
extern "C" {
#endif
	i32 read_maps   (pid_t pid, i32 (*callback)(struct region_map *map,
			 void *data), void *data);
	i32 process_map (struct region_map *map, void *data);
	i32 write_maps_to_file (const char *path, pid_t pid);
	ptr_t get_stack_end (pid_t pid, ptr_t start, ptr_t end);
#ifdef __cplusplus
};
#endif


#if defined(__WINNT__) || defined (__WIN32__)
static inline void get_exe_path_by_pid (pid_t pid, char exe_path[],
					size_t path_size) {}
static inline i32 fopen_maps (FILE *maps, pid_t pid) { return -1; }
#else
/* Assumption: sizeof(exe_path) >= MAPS_MAX_PATH */
static inline void get_exe_path_by_pid (pid_t pid, char exe_path[],
					size_t path_size)
{
	char link_path[128];
	ssize_t ret;

	/* get executable path */
	if (snprintf(link_path, sizeof(link_path), "/proc/%u/exe", pid) <= 0)
		return;
	ret = readlink(link_path, exe_path, path_size - 1);
	if (ret > 0) {
		exe_path[ret] = '\0';
	} else {
		/* readlink() may fail for special processes, treat as empty */
		exe_path[0] = '\0';
	}
}

static inline i32 fopen_maps (FILE **maps, pid_t pid)
{
	char maps_path[128];

	/* construct the maps file path */
	if (snprintf(maps_path, sizeof(maps_path), "/proc/%u/maps", pid) <= 0)
		return -1;

	/* attempt to open the maps file */
	*maps = fopen(maps_path, "r");
	if (!*maps) {
		fprintf(stderr, "Failed to open maps file %s.\n", maps_path);
		return -1;
	}
	return 0;
}
#endif

/* PIE detection - check for known static load addresses */
static inline ptr_t calc_exe_offs (ulong map_start)
{
	ptr_t exe_offs;
#ifdef __arm__
	/* Static load address: armv7l: 0x8000 */
	if (map_start == 0x8000UL)
#else
	/* Static load address: x86: 0x8048000, x86_64: 0x400000 */
	if (map_start == 0x8048000UL ||
	    (map_start == 0x400000UL))
#endif
		exe_offs = 0;
	else
		exe_offs = (ptr_t) map_start;

	return exe_offs;
}

enum region_type {
	REGION_TYPE_MISC,
	REGION_TYPE_CODE,
	REGION_TYPE_EXE,
	REGION_TYPE_HEAP,
	REGION_TYPE_STACK
};

#define REGION_TYPE_NAMES { "misc", "code", "exe", "heap", "stack" }
extern const char *region_type_names[];

/* a region obtained from /proc/$pid/maps */
struct region {
	struct list_head list;
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
	char file_path[1];  /* associated file, must be last */
};

/* process_map() parameters */
struct pmap_params {
	char *exe_path;
	struct list_head *rlist;
};


static inline void list_regions (struct list_head *rlist)
{
	struct region *region;

	clist_for_each_entry (region, rlist, list)
		printf("[%2u] %14lx, %7lu bytes, %5s, "
			"%14lx, %c%c%c, %s\n", region->id,
			region->start, region->size,
			region_type_names[region->type], region->load_addr,
			region->flags.read ? 'r' : '-',
			region->flags.write ? 'w' : '-',
			region->flags.exec ? 'x' : '-',
			region->file_path[0] ? region->file_path : "unassociated");
}

static inline void rlist_clear (struct list_head *rlist)
{
	struct region *region, *tmp;

	clist_for_each_entry_safe (region, tmp, rlist, list) {
		clist_del(&region->list);
		free(region);
	}
}

static inline void read_regions (pid_t pid, struct pmap_params *params)
{
	struct list_head *rlist = params->rlist;

	rlist_clear(rlist);
	if (read_maps(pid, process_map, params))
		rlist_clear(rlist);
}

#endif
