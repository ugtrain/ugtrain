/* maps_l.cpp:    parse /proc/$pid/maps
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <stdbool.h>
#include <unistd.h>
#include <list>
using namespace std;

/* local includes */
#include "maps.h"

/* buffer size for reading symbolic links */
#define MAX_LINKBUF_SIZE 256

const char *region_type_names[] = REGION_TYPE_NAMES;

static inline i32 parse_region (char *line, char *filename, char *exename,
				char *binname, list<struct region> *regions)
{
	ulong start, end;
	struct region *map = NULL;
	char read, write, exec, cow;
	i32 offset, dev_major, dev_minor, inode;
	enum region_type type = REGION_TYPE_MISC;
	static u32 code_regions = 0;
	static bool is_exe = false;
	static ulong prev_end = 0;
	static ulong load_addr = 0;

	if (sscanf(line, "%lx-%lx %c%c%c%c %x %x:%x %u %s", &start, &end,
	    &read, &write, &exec, &cow, &offset, &dev_major, &dev_minor,
	    &inode, filename) < 6)
		goto out;
	/*
	 * get the load address for regions of the same ELF binary
	 *
	 * When a dynamic loader loads an executable or a library into
	 * memory, there is one region per binary segment created:
	 * .text (r-x), .rodata (r--), .data (rw-) and .bss (rw-). The
	 * 'x' permission of .text is used to detect the load address
	 * (region start) and the end of the binary in memory. All
	 * these regions have the same filename. The only exception
	 * is the .bss region. Its filename is empty and it is
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
		if (exec == 'x' || (strncmp(filename, binname,
		  MAX_LINKBUF_SIZE) != 0 && (filename[0] != '\0' ||
		  start != prev_end)) || code_regions >= 4) {
			code_regions = 0;
			is_exe = false;
		} else {
			code_regions++;
		}
	}
	if (code_regions == 0) {
		if (exec == 'x' && filename[0] != '\0') {
			code_regions++;
			if (strncmp(filename, exename, MAX_LINKBUF_SIZE) == 0)
				is_exe = true;
			strncpy(binname, filename, MAX_LINKBUF_SIZE);
			/* terminate just to be sure */
			binname[MAX_LINKBUF_SIZE - 1] = '\0';
		}
		load_addr = start;
	}
	prev_end = end;

	/* must have permissions to read and exec/write and be non-zero size */
	if ((write == 'w' || exec == 'x') && read == 'r' && end - start > 0) {

		/* determine region type */
		if (is_exe)
			type = REGION_TYPE_EXE;
		else if (code_regions > 0)
			type = REGION_TYPE_CODE;
		else if (!strcmp(filename, "[heap]"))
			type = REGION_TYPE_HEAP;
		else if (!strcmp(filename, "[stack]"))
			type = REGION_TYPE_STACK;

		/* allocate a new region structure */
		if ((map = (struct region *) calloc(1, sizeof(struct region) +
		    strlen(filename))) == NULL) {
			cerr << "failed to allocate memory for region."
			     << endl;
			goto error;
		}

		/* initialise this region */
		map->flags.read = true;
		map->flags.write = true;
		map->start = start;
		map->size = end - start;
		map->type = type;
		map->load_addr = load_addr;

		/* setup other permissions */
		map->flags.exec = (exec == 'x');
		map->flags.shared = (cow == 's');
		map->flags.priv = (cow == 'p');
		map->filename = NULL;

		/* save pathname */
		if (strlen(filename) != 0) {
			/* the pathname is concatenated with the structure */
			if ((map = (struct region *) realloc(map,
			    sizeof(*map) + strlen(filename))) == NULL) {
				cerr << "failed to allocate memory." << endl;
				goto error;
			}

			map->filename = new string(filename);
		}

		/* add an unique identifier */
		map->id = regions->size();

		/* okay, add this region to our list */
		regions->push_back(*map);
	}
out:
	return 0;
error:
	return -1;
}

bool readmaps (pid_t pid, list<struct region> *regions)
{
	FILE *maps;
	char name[128], *line = NULL;
	char exelink[128];
	size_t len = 0;
	char linkbuf[MAX_LINKBUF_SIZE], *exename = linkbuf;
	i32 linkbuf_size;
	char binname[MAX_LINKBUF_SIZE];

	/* check if pid is valid */
	if (pid == 0)
		return false;

	/* construct the maps filename */
	snprintf(name, sizeof(name), "/proc/%u/maps", pid);

	/* attempt to open the maps file */
	if ((maps = fopen(name, "r")) == NULL) {
		cerr << "failed to open maps file " << name << "." << endl;
		return false;
	}
	/* get executable name */
	snprintf(exelink, sizeof(exelink), "/proc/%u/exe", pid);
	if ((linkbuf_size = readlink(exelink, exename,
	    MAX_LINKBUF_SIZE)) > 0) {
		exename[linkbuf_size] = 0;
	} else {
		/* readlink may fail for special processes, just treat as empty
		   in order not to miss those regions */
		exename[0] = 0;
	}

	/* read every line of the maps file */
	while (getline(&line, &len, maps) != -1) {
		char *filename;

		/* slight overallocation */
		if ((filename = (char *) alloca(len)) == NULL) {
			cerr << "failed to allocate " << len << " bytes for "
				"filename." << endl;
			goto error;
		}

		/* initialise to zero */
		memset(filename, '\0', len);

		/* parse each line */
		if (parse_region(line, filename, exename, binname,
		    regions) != 0)
			goto error;
	}
	/* release memory allocated */
	free(line);
	fclose(maps);

	return true;

error:
	free(line);
	fclose(maps);

	return false;
}
