/* maps_l.c:    parse /proc/$pid/maps
 *
 * Copyright (C) 2006,2007,2009 Tavis Ormandy <taviso@sdf.lonestar.org>
 * Copyright (C) 2009           Eli Dupree <elidupree@charter.net>
 * Copyright (C) 2009,2010      WANG Lu <coolwanglu@gmail.com>
 * Copyright (c) 2014..15       Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * Code in this file has been taken from scanmem.
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
#define _GNU_SOURCE   /* getline */
#endif

#include <stdio.h>
#include <stdlib.h>    /* free */
#include <string.h>    /* memset */
#include <alloca.h>    /* alloca */

/* local includes */
#include "maps.h"


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
	char maps_path[128];
	char *line = NULL;
	size_t len = 0;
	i32 ret = -1;

	/* construct the maps file path */
	snprintf(maps_path, sizeof(maps_path), "/proc/%u/maps", pid);

	/* attempt to open the maps file */
	maps = fopen(maps_path, "r");
	if (!maps) {
		fprintf(stderr, "Failed to open maps file %s.\n", maps_path);
		goto out;
	}
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
