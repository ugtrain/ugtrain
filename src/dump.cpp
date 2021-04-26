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

#include <algorithm>
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
	ugout << ">>> Dumping Pointer " << main_id		\
	      << " Obj " << obj_id << " at 0x"			\
	      << hex << it->old_val.ptr << dec << endl;		\
	dump_mem_obj(pid, mfd, "p", main_id, obj_id,		\
		     it->old_val.ptr, it->ptrtgt->mem_size);	\
} while (0)

#define CHECK_PTR(__jump_target)				\
do {								\
	if (it->old_val.u64 == 0 ||				\
	    it->ptrtgt->v_state[obj_id] < PTR_SETTLED)		\
		goto __jump_target;				\
} while (0)

void dump_all_mem_obj (pid_t pid, Options *opt)
{
	DynMemEntry *old_dynmem = NULL;
	u32 main_id = 0, obj_id = 0, i;
	i32 mfd = opt->procmem_fd;
	list<CfgEntry> *cfg = opt->cfg;
	list<CfgEntry>::iterator it;
	list<DumpEntry> *dump_list = opt->dump_list;
	list<DumpEntry>::iterator dit;

	dump_maps(pid);

	main_id = 0;  // class id
	list_for_each (cfg, it) {
		if (!it->dynmem || it->dynmem == old_dynmem)
			continue;
		vector<size_t> *svec = &it->dynmem->v_msize;
		vector<DynMemEssentials>::iterator esit;
		obj_id = 0;
		for (i = 0; i < it->dynmem->v_maddr.size(); i++) {
			ugout << ">>> Dumping Class " << main_id
			      << " Obj " << obj_id << " at 0x"
			      << hex << it->dynmem->v_maddr[obj_id]
			      << dec << endl;
			dump_mem_obj(pid, mfd, NULL, main_id, obj_id,
				     it->dynmem->v_maddr[obj_id],
				     svec->at(obj_id));
			obj_id++;
		}
		main_id++;
		old_dynmem = it->dynmem;
	}

	main_id = 0;  // pointer id
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
		main_id++;
	}

	// Static memory areas
	main_id = 0;  // area id
	obj_id = 0;
	list_for_each (dump_list, dit) {
		// Handle late PIC
		if (dit->lib && !dit->lib->is_loaded)
			goto skip_statmem;
		ugout << ">>> Dumping Static Memory " << main_id
		      << " Obj " << obj_id << " at 0x"
		      << hex << dit->addr << dec << endl;
		dump_mem_obj(pid, mfd, "s", main_id, obj_id,
			     dit->addr, dit->mem_size);
skip_statmem:
		main_id++;
	}
}
