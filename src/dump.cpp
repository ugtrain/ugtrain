/* dump.cpp:    dump dynamic memory objects in process memory
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * powered by the Open Game Cheating Association
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
#include <lib/memattach.h>
#include <common.h>
#include <commont.cpp>
#include <dump.h>


static void dump_ptr_mem (pid_t pid, u32 obj_id, u32 ptr_id,
			  ptr_t mem_addr, size_t size)
{
	i32 fd;
	string fname;
	char buf[size];
	ssize_t wbytes;

	if (memattach(pid) != 0)
		goto err;
	if (memread(pid, mem_addr, buf, sizeof(buf)) != 0)
		goto err_detach;
	memdetach(pid);

	fname += "p";
	fname += to_string(ptr_id);
	fname += "_";
	if (obj_id < 100)
		fname += "0";
	if (obj_id < 10)
		fname += "0";
	fname += to_string(obj_id);
	fname += ".dump";

	fd = open(fname.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0)
		goto err;
	wbytes = write(fd, buf, sizeof(buf));
	if (wbytes < (ssize_t) sizeof(buf))
		cerr << fname << ": Write error!" << endl;
	close(fd);
	return;
err_detach:
	memdetach(pid);
err:
	return;
}

static void dump_mem_obj (pid_t pid, u32 class_id, u32 obj_id,
			  ptr_t mem_addr, size_t size)
{
	i32 fd, ret;
	string fname, backup;
	char buf[size];
	ssize_t wbytes;

	if (memattach(pid) != 0)
		goto err;
	if (memread(pid, mem_addr, buf, sizeof(buf)) != 0)
		goto err_detach;
	memdetach(pid);

	fname = to_string(class_id);
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
			cerr << "renaming " << fname << " to "
			     << backup << " failed!" << endl;
			goto err;
		}
	}
	fd = open(fname.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd < 0)
		goto err;
	wbytes = write(fd, buf, sizeof(buf));
	if (wbytes < (ssize_t) sizeof(buf))
		cerr << fname << ": Write error!" << endl;
	close(fd);
	return;
err_detach:
	memdetach(pid);
err:
	return;
}

void dump_all_mem_obj (pid_t pid, list<CfgEntry> *cfg)
{
	DynMemEntry *old_dynmem = NULL;
	u32 class_id = 0, obj_id = 0, ptr_id = 0, i;
	list<CfgEntry>::iterator it;

	list_for_each (cfg, it) {
		if (it->dynmem && it->dynmem != old_dynmem) {
			obj_id = 0;
			for (i = 0; i < it->dynmem->v_maddr.size(); i++) {
				cout << ">>> Dumping Class " << class_id
				     << " Obj " << obj_id << " at 0x"
				     << hex << it->dynmem->v_maddr[obj_id]
				     << dec << endl;
				dump_mem_obj(pid, class_id, obj_id,
					     it->dynmem->v_maddr[obj_id],
					     it->dynmem->mem_size);
				obj_id++;
			}
			class_id++;
			old_dynmem = it->dynmem;
		}
	}

	list_for_each (cfg, it) {
		if (it->dynmem && it->ptrtgt) {
			obj_id = 0;
			for (i = 0; i < it->dynmem->v_maddr.size(); i++) {
				if (it->v_oldval[obj_id].u64 > 0 &&
				    it->ptrtgt->v_state[obj_id] >= PTR_SETTLED)
					dump_ptr_mem(pid, obj_id, ptr_id,
						     it->v_oldval[obj_id].ptr,
						     it->ptrtgt->mem_size);
				obj_id++;
			}
			ptr_id++;
		}
	}
}
