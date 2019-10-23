/* dump.cpp:    dump dynamic memory objects in process memory
 *
 * Copyright (c) 2012..2019 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

// local includes
#include <lib/maps.h>
#include <lib/memattach.h>
#include <common.h>
#include <commont.cpp>
#include <dump.h>


static void dump_mem_obj (pid_t pid, i32 mfd, const char *prefix, u32 main_id,
			  u32 obj_id, ptr_t mem_addr, size_t size)
{
	i32 fd, ret;
	string fname, backup;
	char buf[size];
	ssize_t wbytes;

	if (memattach(pid) != 0)
		goto err;
	if (memread((mfd >= 0) ? mfd : pid, mem_addr, buf, sizeof(buf)) != 0)
		goto err_detach;
	memdetach(pid);

	if (prefix)
		fname += prefix;
	fname += to_string(main_id);
	fname += "_";
	if (obj_id < 100)
		fname += "0";
	if (obj_id < 10)
		fname += "0";
	fname += to_string(obj_id);
	backup = fname;
	backup += "~.dump";
	fname += ".dump";

	// store a backup if file exists for keeping two states to compare
	fd = open(fname.c_str(), O_RDONLY);
	if (fd >= 0) {
		close(fd);
		ret = rename(fname.c_str(), backup.c_str());
		if (ret) {
			ugerr << "Renaming " << fname << " to "
			      << backup << " failed!" << endl;
			goto err;
		}
	}
	fd = open(fname.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0)
		goto err;
	wbytes = write(fd, buf, sizeof(buf));
	if (wbytes < (ssize_t) sizeof(buf))
		ugerr << fname << ": Write error!" << endl;
	close(fd);
	return;
err_detach:
	memdetach(pid);
err:
	return;
}

static inline void dump_maps (pid_t pid)
{
	i32 fd, ret;
	string fname = "maps.dump.txt";
	string backup = "maps~.dump.txt";

	// store a backup if file exists for keeping two states to compare
	fd = open(fname.c_str(), O_RDONLY);
	if (fd >= 0) {
		close(fd);
		ret = rename(fname.c_str(), backup.c_str());
		if (ret) {
			ugerr << "Renaming " << fname << " to "
			      << backup << " failed!" << endl;
			goto out;
		}
	}
	ugout << ">>> Dumping memory maps to " << fname << endl;
	write_maps_to_file(fname.c_str(), pid);
out:
	return;
}

#define DUMP_PTR_MEM()						\
do {								\
	ugout << ">>> Dumping Pointer " << ptr_id		\
	      << " Obj " << obj_id << " at 0x"			\
	      << hex << it->old_val.ptr << dec << endl;		\
	dump_mem_obj(pid, mfd, "p", ptr_id, obj_id,		\
		     it->old_val.ptr, it->ptrtgt->mem_size);	\
} while (0)

#define CHECK_PTR(__jump_target)				\
do {								\
	if (it->old_val.u64 == 0 ||				\
	    it->ptrtgt->v_state[obj_id] < PTR_SETTLED)		\
		goto __jump_target;				\
} while (0)

void dump_all_mem_obj (pid_t pid, i32 mfd, list<CfgEntry> *cfg)
{
	DynMemEntry *old_dynmem = NULL;
	u32 class_id = 0, obj_id = 0, ptr_id = 0, i;
	list<CfgEntry>::iterator it;

	dump_maps(pid);

	list_for_each (cfg, it) {
		if (!it->dynmem || it->dynmem == old_dynmem)
			continue;
		obj_id = 0;
		for (i = 0; i < it->dynmem->v_maddr.size(); i++) {
			ugout << ">>> Dumping Class " << class_id
			      << " Obj " << obj_id << " at 0x"
			      << hex << it->dynmem->v_maddr[obj_id]
			      << dec << endl;
			dump_mem_obj(pid, mfd, NULL, class_id, obj_id,
				     it->dynmem->v_maddr[obj_id],
				     it->dynmem->mem_size);
			obj_id++;
		}
		class_id++;
		old_dynmem = it->dynmem;
	}

	list_for_each (cfg, it) {
		if (!it->ptrtgt)
			continue;
		obj_id = 0;
		if (!it->dynmem) {
			CHECK_PTR(skip_ptr);
			DUMP_PTR_MEM();
			goto skip_ptr;
		}
		for (i = 0; i < it->dynmem->v_maddr.size(); i++) {
			it->old_val = it->v_oldval[obj_id];
			CHECK_PTR(skip_obj);
			DUMP_PTR_MEM();
skip_obj:
			obj_id++;
		}
skip_ptr:
		ptr_id++;
	}
}
