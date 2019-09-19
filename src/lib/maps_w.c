/* maps_w.c:    just a stub
 *
 * Copyright (C) 2014..2019     Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#if defined(__WINNT__) || defined (__WIN32__)

/* local includes */
#include "maps.h"


i32 write_maps_to_file (const char *path, pid_t pid)
{
	return -1;
}

const char *region_type_names[] = REGION_TYPE_NAMES;

/*
 * callback for read_maps()
 */
i32 process_map (struct region_map *map, void *data)
{
	return -1;
}

/*
 * returns:
 *  0: all memory maps successfully processed,
 * -1: fopen(), alloca() or sscanf() error,
 * return code of callback() != 0.
 */
i32 read_maps (pid_t pid, i32 (*callback)(struct region_map *map, void *data),
	       void *data)
{
	return -1;
}

ptr_t get_stack_end (pid_t pid, ptr_t start, ptr_t end)
{
	return start;
}

#endif
