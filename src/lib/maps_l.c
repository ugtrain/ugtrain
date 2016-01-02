/* maps_l.c:    parse /proc/$pid/maps
 *
 * Copyright (C) 2006,2007,2009 Tavis Ormandy <taviso@sdf.lonestar.org>
 * Copyright (C) 2009           Eli Dupree <elidupree@charter.net>
 * Copyright (C) 2009,2010      WANG Lu <coolwanglu@gmail.com>
 * Copyright (C) 2014..2015     Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * Code in this file has been taken from scanmem.
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE   /* getline */
#endif

#include <stdio.h>
#include <stdlib.h>    /* free */
#include <string.h>    /* memset */
#include <alloca.h>    /* alloca */

/* local includes */
#include "maps.h"


const char *region_type_names[] = REGION_TYPE_NAMES;

/*
 * callback for read_maps()
 *
 * append all executable and writable regions to a regions list
 */
i32 process_map (struct map *map, void *data)
{
	struct pmap_params *params = (struct pmap_params *) data;
	char *exe_path = params->exe_path;
	struct list_head *rlist = params->rlist;
	struct region *region = NULL;
	enum region_type type = REGION_TYPE_MISC;
	static char code_path[MAPS_MAX_PATH];
	static u32 code_regions = 0, exe_regions = 0;
	static bool is_exe = false;
	static ulong prev_end = 0;
	static ulong load_addr = 0, exe_load = 0;

	/*
	 * get the load address for regions of the same ELF file
	 *
	 * When the ELF loader loads an executable or a library into
	 * memory, there is one region per ELF segment created:
	 * .text (r-x), .rodata (r--), .data (rw-) and .bss (rw-). The
	 * 'x' permission of .text is used to detect the load address
	 * (region start) and the end of the ELF file in memory. All
	 * these regions have the same filename. The only exception
	 * is the .bss region. Its filename is empty and it is
	 * consecutive with the .data region. But the regions .bss and
	 * .rodata may not be present with some ELF files. This is why
	 * we can't rely on other regions to be consecutive in memory.
	 * There should never be more than these four regions.
	 * The data regions use their variables relative to the load
	 * address. So determining it makes sense as we can get the
	 * variable address used within the ELF file with it.
	 * But for the executable there is the special case that there
	 * is a gap between .text and .rodata. Other regions might be
	 * loaded via mmap() to it. So we have to count the number of
	 * regions belonging to the exe separately to handle that.
	 * References:
	 * http://en.wikipedia.org/wiki/Executable_and_Linkable_Format
	 * http://wiki.osdev.org/ELF
	 * http://lwn.net/Articles/531148/
	 */

	/* detect further regions of the same ELF file and its end */
	if (code_regions > 0) {
		if (map->exec == 'x' || (strncmp(map->file_path, code_path,
		    MAPS_MAX_PATH) != 0 && (map->file_path[0] != '\0' ||
		    map->start != prev_end)) || code_regions >= 4) {
			code_regions = 0;
			is_exe = false;
			/* exe with .text and without .data is impossible */
			if (exe_regions > 1)
				exe_regions = 0;
		} else {
			code_regions++;
			if (is_exe)
				exe_regions++;
		}
	}
	if (code_regions == 0) {
		/* detect the first region belonging to an ELF file */
		if (map->exec == 'x' && map->file_path[0] != '\0') {
			code_regions++;
			if (strncmp(map->file_path, exe_path,
			    MAPS_MAX_PATH) == 0) {
				exe_regions = 1;
				exe_load = map->start;
				is_exe = true;
			}
			strncpy(code_path, map->file_path, MAPS_MAX_PATH);
			/* terminate just to be sure */
			code_path[MAPS_MAX_PATH - 1] = '\0';
		/* detect the second region of the exe after skipping regions */
		} else if (exe_regions == 1 && map->file_path[0] != '\0' &&
		    strncmp(map->file_path, exe_path, MAPS_MAX_PATH) == 0) {
			code_regions = ++exe_regions;
			load_addr = exe_load;
			is_exe = true;
			strncpy(code_path, map->file_path, MAPS_MAX_PATH);
			/* terminate just to be sure */
			code_path[MAPS_MAX_PATH - 1] = '\0';
		}
		if (exe_regions < 2)
			load_addr = map->start;
	}
	prev_end = map->end;

	/* must have permissions to read and exec/write and be non-zero size */
	if ((map->write == 'w' || map->exec == 'x') && map->read == 'r' &&
	    map->end - map->start > 0) {

		/* determine region type */
		if (is_exe)
			type = REGION_TYPE_EXE;
		else if (code_regions > 0)
			type = REGION_TYPE_CODE;
		else if (!strcmp(map->file_path, "[heap]"))
			type = REGION_TYPE_HEAP;
		else if (!strcmp(map->file_path, "[stack]"))
			type = REGION_TYPE_STACK;

		/* allocate a new region structure */
		region = (struct region *) calloc(1, sizeof(struct region) +
			strlen(map->file_path));
		if (!region) {
			fprintf(stderr, "Failed to allocate memory for "
				"region.\n");
			goto error;
		}

		/* initialise this region */
		region->flags.read = true;
		region->flags.write = true;
		region->start = map->start;
		region->size = map->end - map->start;
		region->type = type;
		region->load_addr = load_addr;

		/* setup other permissions */
		region->flags.exec = (map->exec == 'x');
		region->flags.shared = (map->cow == 's');
		region->flags.priv = (map->cow == 'p');

		/* save pathname */
		if (strlen(map->file_path) != 0) {
			/* the pathname is concatenated with the structure */
			strcpy(region->file_path, map->file_path);
		}

		/* add an unique identifier */
		if (clist_empty(rlist))
			region->id = 0;
		else
			region->id = clist_last_entry(rlist,
				struct region, list)->id + 1;

		/* okay, add this region to our list */
		clist_add_tail(&region->list, rlist);
	}
	return 0;
error:
	return -1;
}

/*
 * Read all maps from /proc/pid/maps, parse them and
 * call a callback per line.
 *
 * returns:
 *  0: all memory maps successfully processed,
 * -1: fopen(), alloca() or sscanf() error,
 * return code of callback() != 0.
 *
 * Assumption: pid > 0
 * Inspired by dl_iterate_phdr().
 */
i32 read_maps (pid_t pid, i32 (*callback)(struct map *map, void *data),
	       void *data)
{
	FILE *maps;
	char *line = NULL;
	size_t len = 0;
	i32 ret = -1;

	/* attempt to open the maps file */
	ret = fopen_maps(&maps, pid);
	if (ret)
		goto out;

	/* read every line of the maps file, parse them and call the callback */
	while (getline(&line, &len, maps) != -1) {
		struct map map;

		/* slight overallocation */
		map.file_path = (char *) alloca(len);
		if (!map.file_path) {
			fprintf(stderr, "Failed to allocate %zu bytes for "
				"file path.\n", len);
			goto err_free;
		}
		memset(map.file_path, '\0', len);

		if (sscanf(line, "%lx-%lx %c%c%c%c %x %x:%x %u %s", &map.start,
		    &map.end, &map.read, &map.write, &map.exec, &map.cow,
		    &map.offset, &map.dev_major, &map.dev_minor, &map.inode,
		    map.file_path) < 6)
			goto err_free;

		/* process each line by map structure */
		ret = callback(&map, data);
		if (ret)
			goto out_free;
	}
out_free:
	free(line);
	fclose(maps);
out:
	return ret;
err_free:
	ret = -1;
	goto out_free;
}
