/* aslr.cpp:    handle Address Space Layout Randomization (ASLR)
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * By the original authors of ugtrain there shall be ABSOLUTELY
 * NO RESPONSIBILITY or LIABILITY for derived work and/or abusive
 * or malicious use. The ugtrain is an education project and
 * sticks to the copyright law by providing configs for games
 * which ALLOW CHEATING only. We can't and we don't accept abusive
 * configs or codes which might turn ugtrain into a cracker tool!
 */

#include <cstring>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>  // basename

// local includes
#include <aslr.h>
#include <fifoparser.h>


// ########################################
// ########## late PIC handling ###########
// ########################################

// ##### Memory Discovery #####

static inline void send_lib_bounds (i32 ofd, ptr_t lib_start, ptr_t lib_end)
{
	char obuf[PIPE_BUF];
	i32 osize = 0;
	ssize_t wbytes;

	osize += snprintf(obuf + osize, sizeof(obuf) - osize,
		PRI_PTR ";" PRI_PTR "\n", lib_start, lib_end);

	wbytes = write(ofd, obuf, osize);
	if (wbytes < osize)
		perror("FIFO write");
}

/*
 * callback for read_maps(),
 * searches for disc_lib start and end code address in memory,
 * sends found code addresses as the new backtrace filter to libmemdisc,
 * returns 1 for stopping the iteration (disc_lib found) and 0 otherwise
 *
 * Assumptions: disc_lib != NULL, disc_lib[0] != '\0'
 */
static inline i32 send_bt_addrs (struct map *map, void *data)
{
	struct lf_disc_params *lfp = (struct lf_disc_params *) data;
	char *disc_lib = lfp->disc_lib;
	i32 ofd = lfp->ofd;
	char *lib_name = basename(map->file_path);
	ptr_t lib_start, lib_end;

	if (map->exec != 'x')
		goto out;
	if (!strstr(lib_name, disc_lib))
		goto out;

	lib_start = (ptr_t) map->start;
	lib_end = (ptr_t) map->end;
	send_lib_bounds(ofd, lib_start, lib_end);
	return 1;
out:
	return 0;
}

// lf() callback for read_dynmem_buf()
static void handle_disc_pic (LF_PARAMS)
{
	struct lf_disc_params *lfp = (struct lf_disc_params *) argp;
	pid_t pid = lfp->pid;
	i32 ofd = lfp->ofd;
	char *disc_lib = lfp->disc_lib;
	i32 ret;

	//cout << lib_name << " loaded" << endl;

	if (disc_lib && disc_lib[0] != '\0' &&
	    strstr(lib_name, disc_lib)) {
		ret = read_maps(pid, send_bt_addrs, lfp);
		if (!ret)
			send_lib_bounds(ofd, 0, UINTPTR_MAX);
	}
}

/*
 * returns:
 *  0: data has been read
 * -1: no data read
 */
static inline i32 read_libs_from_fifo (pid_t pid, char *disc_lib,
				       i32 ifd, i32 ofd)
{
	ssize_t rbytes;
	struct parse_cb pcb = { NULL };
	struct lf_disc_params lfp;
	i32 i, ret = -1;

	pcb.lf = handle_disc_pic;
	lfp.pid = pid;
	lfp.ofd = ofd;
	lfp.disc_lib = disc_lib;

	for (i = 0; ; i++) {
		rbytes = read_dynmem_buf(NULL, &lfp, ifd, 0, false, 0, &pcb);
		if (rbytes <= 0)
			break;
		if (i == 0)
			ret = 0;
	}
	return ret;
}

/*
 * Wait for the game process and handle PIC in discovery
 *
 * Reading the regions list upon every library load would be too much.
 * Most libraries are loaded consecutively during game start. So do it
 * after some cycles of no input from the FIFO.
 */
void do_disc_pic_work (pid_t pid, struct app_options *opt,
		       i32 ifd, i32 ofd, struct list_head *rlist)
{
#define CYCLES_BEFORE_RELOAD 2
	struct pmap_params params;
	char exe_path[MAPS_MAX_PATH];
	enum pstate pstate;
	i32 i = 0, ret;

	get_exe_path_by_pid(pid, exe_path, sizeof(exe_path));
	params.exe_path = exe_path;
	params.rlist = rlist;

	while (true) {
		ret = read_libs_from_fifo(pid, opt->disc_lib, ifd, ofd);
		if (i && ret) {
			i--;
			if (!i)
				read_regions(pid, &params);
		} else if (!ret) {
			i = CYCLES_BEFORE_RELOAD;
		}
		pstate = check_process(pid, opt->proc_name);
		if (pstate != PROC_RUNNING && pstate != PROC_ERR)
			return;
		sleep_sec_unless_input(1, ifd);
	}
#undef CYCLES_BEFORE_RELOAD
}

// ########################################
// ###### PIE and early PIC handling ######
// ########################################

/*
 * Libraries are always built as position independent code (PIC).
 * So we have to find the start and the end of their code regions.
 */
static inline void find_lib_region (struct list_head *rlist, char *lib,
				    ptr_t *lib_start, ptr_t *lib_end)
{
	struct region *it;

	clist_for_each_entry (it, rlist, list) {
		char *file_name;
		if (!it->flags.exec || it->type == REGION_TYPE_EXE)
		       continue;
		file_name = basename(it->file_path);
		if (strstr(file_name, lib)) {
			*lib_start = (ptr_t) it->start;
			*lib_end = (ptr_t) (it->start + it->size);
			return;
		}
	}
	cout << "Couldn't find load address of " << lib
	     << " for early PIC." << endl;

	*lib_start = 0;
	*lib_end = UINTPTR_MAX;
}

/*
 * Some Linux distributions use the GCC options -pie and -fPIE
 * by default for hardened security. Together with ASLR, the executable
 * is loaded to another memory location/address each execution. We
 * have to find out that memory address and use it as an offset. This
 * applies to code addresses on the stack and static memory as well.
 * PIE is detected from that load address.
 */
static inline ptr_t get_exe_offs (struct region *r)
{
	/* PIE detection */
	ptr_t exe_offs = calc_exe_offs(r->start);

	cout << "exe_offs: 0x" << hex << exe_offs << dec << endl;
	if (exe_offs)
		cout << "PIE (position independent executable) detected!"
		     << endl;
	return exe_offs;
}

static inline void find_exe_region (struct list_head *rlist,
				    ptr_t *exe_start, ptr_t *exe_end,
				    ptr_t *exe_offs)
{
	struct region *it;

	clist_for_each_entry (it, rlist, list) {
		if (it->type == REGION_TYPE_EXE && it->flags.exec) {
			*exe_start = (ptr_t) it->start;
			*exe_end = (ptr_t) (it->start + it->size);
			*exe_offs = get_exe_offs(&(*it));
			break;
		}
	}
}

/*
 * handle PIE and early PIC
 * for memory discovery and hacking
 */
void handle_pie (struct app_options *opt, list<CfgEntry> *cfg, i32 ifd,
		 i32 ofd, pid_t pid, struct list_head *rlist)
{
	struct pmap_params params;
	char buf[PIPE_BUF];
	ssize_t rbytes;
	list<CfgEntry>::iterator cfg_it;
	DynMemEntry *old_dynmem = NULL;
	ptr_t exe_offs = 0, exe_start = 0, exe_end = 0;
	ssize_t wbytes;
	char exe_path[MAPS_MAX_PATH];
	char obuf[PIPE_BUF];
	i32 osize = 0;

	if (!opt->pure_statmem) {
		while (true) {
			sleep_sec_unless_input(1, ifd);
			rbytes = read(ifd, buf, sizeof(buf));
			if (rbytes == 0 ||
			    (rbytes < 0 && errno == EAGAIN))
				continue;
			else
				break;
		}
	}
	get_exe_path_by_pid(pid, exe_path, sizeof(exe_path));
	params.exe_path = exe_path;
	params.rlist = rlist;
	read_regions(pid, &params);
	find_exe_region(rlist, &exe_start, &exe_end, &exe_offs);
	opt->code_offs = exe_offs;

	if (opt->pure_statmem)
		return;
	if (opt->disc_str) {
		if (!opt->disc_lib)
			return;
		if (opt->disc_lib[0] == '\0') {   // PIE
			osize += snprintf(obuf + osize, sizeof(obuf) - osize,
				PRI_PTR ";" PRI_PTR ";" PRI_PTR "\n", exe_start,
				exe_end, exe_offs);
		} else {                          // early PIC
			ptr_t lib_start, lib_end;

			find_lib_region(rlist, opt->disc_lib, &lib_start,
				&lib_end);
			// notify libmemdisc that we are ready for PIC handling
			// and send backtrace filter for early library load
			osize += snprintf(obuf + osize, sizeof(obuf) - osize,
				PRI_PTR ";" PRI_PTR "\n", lib_start, lib_end);
		}
	} else {
		// handle PIE and early PIC for libmemhack
		list_for_each (cfg, cfg_it) {
			ptr_t code_offs = 0, lib_end;
			DynMemEntry *dynmem = cfg_it->dynmem;
			GrowEntry *grow;
			if (!dynmem || dynmem == old_dynmem)
				continue;
			grow = dynmem->grow;
			old_dynmem = dynmem;
			if (!dynmem->lib)
				code_offs = exe_offs;
			else
				find_lib_region(rlist, dynmem->lib, &code_offs,
					&lib_end);
			osize += snprintf(obuf + osize, sizeof(obuf) - osize,
				PRI_PTR ";", code_offs);
			dynmem->code_addr += code_offs;
			if (grow) {
				if (!grow->lib)
					code_offs = exe_offs;
				else
					find_lib_region(rlist, grow->lib,
						&code_offs, &lib_end);
				osize += snprintf(obuf + osize,
					sizeof(obuf) - osize,
					PRI_PTR ";", code_offs);
				grow->code_addr += code_offs;
			}
		}
	}
	// Write code offsets to output FIFO
	wbytes = write(ofd, obuf, osize);
	if (wbytes < osize)
		perror("FIFO write");
}
