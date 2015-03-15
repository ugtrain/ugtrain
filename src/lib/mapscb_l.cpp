/* mapscb_l.cpp:    C++ callbacks for a parsed map from /proc/$pid/maps
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

#include <stdio.h>
#include <stdlib.h>   /* calloc */
#include <string.h>   /* strncmp */
#include <list>
using namespace std;

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
	list<struct region> *rlist = params->rlist;
	struct region *region = NULL;
	enum region_type type = REGION_TYPE_MISC;
	static char code_path[MAPS_MAX_PATH];
	static u32 code_regions = 0;
	static bool is_exe = false;
	static ulong prev_end = 0;
	static ulong load_addr = 0;

	/*
	 * get the load address for regions of the same ELF binary
	 *
	 * When a dynamic loader loads an executable or a library into
	 * memory, there is one region per binary segment created:
	 * .text (r-x), .rodata (r--), .data (rw-) and .bss (rw-). The
	 * 'x' permission of .text is used to detect the load address
	 * (region start) and the end of the binary in memory. All
	 * these regions have the same file path. The only exception
	 * is the .bss region. Its file path is empty and it is
	 * consecutive with the .data region. But the regions .bss and
	 * .rodata may not be present with some binaries. This is why
	 * we can't rely on other regions to be consecutive in memory.
	 * There should never be more than these four regions.
	 * The data regions use their variables relative to the load
	 * address. So determining it makes sense as we can get the
	 * variable address used within the binariy with it.
	 * References:
	 * http://en.wikipedia.org/wiki/Executable_and_Linkable_Format
	 * http://wiki.osdev.org/ELF
	 * http://lwn.net/Articles/531148/
	 */
	if (code_regions > 0) {
		if (map->exec == 'x' || (strncmp(map->file_path, code_path,
		    MAPS_MAX_PATH) != 0 && (map->file_path[0] != '\0' ||
		    map->start != prev_end)) || code_regions >= 4) {
			code_regions = 0;
			is_exe = false;
		} else {
			code_regions++;
		}
	}
	if (code_regions == 0) {
		if (map->exec == 'x' && map->file_path[0] != '\0') {
			code_regions++;
			if (strncmp(map->file_path, exe_path,
			    MAPS_MAX_PATH) == 0)
				is_exe = true;
			strncpy(code_path, map->file_path, MAPS_MAX_PATH);
			/* terminate just to be sure */
			code_path[MAPS_MAX_PATH - 1] = '\0';
		}
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
			cerr << "Failed to allocate memory for region."
			     << endl;
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
		region->file_path = NULL;

		/* save pathname */
		if (strlen(map->file_path) != 0) {
			/* the pathname is concatenated with the structure */
			region = (struct region *) realloc(region,
				sizeof(*region) + strlen(map->file_path));
			if (!region) {
				cerr << "Failed to allocate memory." << endl;
				goto error;
			}

			region->file_path = new string(map->file_path);
		}

		/* add an unique identifier */
		region->id = rlist->size();

		/* okay, add this region to our list */
		rlist->push_back(*region);
	}
	return 0;
error:
	return -1;
}
