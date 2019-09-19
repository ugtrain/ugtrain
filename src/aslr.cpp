/* aslr.cpp:    handle Address Space Layout Randomization (ASLR)
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
#include <fstream>

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
 * Assumptions: disc_lib != NULL, disc_lib not empty, disc_lib[0] != '\1'
 */
static inline i32 send_bt_addrs (struct region_map *map, void *data)
{
	struct lf_disc_params *lfp = (struct lf_disc_params *) data;
	string *disc_lib = lfp->disc_lib;
	i32 ofd = lfp->ofd;
	string lib_name = string(basename(map->file_path));
	ptr_t lib_start, lib_end;

	if (map->exec != 'x')
		goto out;
	if (lib_name != *disc_lib)
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
	string *disc_lib = lfp->disc_lib;
	i32 ret;

	//ugout << *lib_name << " loaded" << endl;

	if (!disc_lib->empty() && disc_lib->at(0) != '\1' &&
	    *lib_name == *disc_lib) {
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
static inline i32
read_libs_from_fifo (pid_t pid, string *disc_lib,
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
void do_disc_pic_work (pid_t pid, Options *opt,
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
		pstate = check_process(pid, opt->proc_name->c_str());
		if (pstate != PROC_RUNNING && pstate != PROC_ERR)
			return;
		sleep_sec_unless_input(1, ifd);
	}
#undef CYCLES_BEFORE_RELOAD
}

// ##### Memory Hacking #####

/*
 * Libraries are always built as position-independent code (PIC).
 * So we have to find the start and the end of their code regions.
 */
static inline void find_lib_region (struct list_head *rlist, const string *lib,
				    ptr_t *lib_start, ptr_t *lib_end)
{
	struct region *it;

	clist_for_each_entry (it, rlist, list) {
		string file_name;
		if (!it->flags.exec || it->type == REGION_TYPE_EXE)
		       continue;
		file_name = string(basename(it->file_path));
		if (file_name == *lib) {
			*lib_start = (ptr_t) it->start;
			*lib_end = (ptr_t) (it->start + it->size);
			return;
		}
	}

	*lib_start = 0;
	*lib_end = UINTPTR_MAX;
}

// lf() callback for read_dynmem_buf()
void get_lib_load_addr (LF_PARAMS)
{
	struct dynmem_params *dmparams = (struct dynmem_params *) argp;
	struct lf_params *lfparams = dmparams->lfparams;
	list<CfgEntry> *cfg = lfparams->cfg;
	pid_t pid = lfparams->pid;
	i32 ofd = lfparams->ofd;
	struct list_head *rlist = lfparams->rlist;
	ptr_t lib_start = 0, lib_end = 0;
	list<CfgEntry>::iterator cfg_it;
	DynMemEntry *old_dynmem = NULL;

	// reload the memory regions
	get_regions(pid, rlist);

	// update the game trainer config
	find_lib_region(rlist, lib_name, &lib_start, &lib_end);
	if (!lib_start)
		goto out;
	list_for_each (cfg, cfg_it) {
		DynMemEntry *dynmem = cfg_it->dynmem;
		GrowEntry *grow;
		if (!dynmem || dynmem == old_dynmem)
			continue;
		grow = dynmem->grow;
		old_dynmem = dynmem;
		if (dynmem->lib && *dynmem->lib == *lib_name)
			dynmem->code_addr = dynmem->code_offs + lib_start;
		if (grow && grow->lib && *grow->lib == *lib_name)
			grow->code_addr = grow->code_offs + lib_start;
	}
	send_lib_bounds(ofd, lib_start, lib_end);
out:
	return;
}

// ########################################
// ###### PIE and early PIC handling ######
// ########################################

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

	ugout << "exe_offs: 0x" << hex << exe_offs << dec << endl;
	if (exe_offs)
		ugout << "PIE (position-independent executable) detected!"
		      << endl;
	return exe_offs;
}

static inline void find_exe_region (struct list_head *rlist,
				    ptr_t *exe_start, ptr_t *exe_end,
				    ptr_t *exe_offs)
{
	struct region *it;

	clist_for_each_entry (it, rlist, list) {
		if (!(it->type == REGION_TYPE_EXE && it->flags.exec))
			continue;
		*exe_start = (ptr_t) it->start;
		*exe_end = (ptr_t) (it->start + it->size);
		*exe_offs = get_exe_offs(&(*it));
		break;
	}
}

/*
 * handle PIE and early PIC
 * for dynamic memory discovery and hacking
 */
void handle_aslr (Options *opt, list<CfgEntry> *cfg, i32 ifd,
		  i32 ofd, pid_t pid, struct list_head *rlist)
{
	char buf[PIPE_BUF];
	ssize_t rbytes;
	list<CfgEntry>::iterator cfg_it;
	DynMemEntry *old_dynmem = NULL;
	ptr_t exe_offs = 0, exe_start = 0, exe_end = 0;
	ssize_t wbytes;
	char obuf[PIPE_BUF];
	i32 osize = 0;

	if (!opt->pure_statmem) {
		while (true) {
			sleep_sec_unless_input(1, ifd);
			rbytes = read(ifd, buf, sizeof(ASLR_NOTIFY_STR));
			if (rbytes == 0 ||
			    (rbytes < 0 && errno == EAGAIN))
				continue;
			else
				break;
		}
	}
	get_regions(pid, rlist);
	find_exe_region(rlist, &exe_start, &exe_end, &exe_offs);
	find_lib_regions(rlist, opt);
	opt->code_offs = exe_offs;

	if (opt->pure_statmem)
		return;
	if (!opt->disc_str->empty()) {
		if (opt->disc_lib->empty())
			return;
		if (opt->disc_lib->at(0) == '\1') {   // PIE
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
			if (!dynmem->lib || dynmem->lib->empty())
				code_offs = exe_offs;
			else
				find_lib_region(rlist, dynmem->lib, &code_offs,
					&lib_end);
			osize += snprintf(obuf + osize, sizeof(obuf) - osize,
				PRI_PTR ";", code_offs);
			dynmem->code_addr += code_offs;
			if (grow) {
				if (!grow->lib || grow->lib->empty())
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

// ############################
// ###### Stack handling ######
// ############################

/*
 * read the stack region bounds from /proc/$pid/maps
 * and store them in opt
 */
void get_stack_bounds (Options *opt, pid_t pid,
		       struct list_head *rlist)
{
	get_regions(pid, rlist);
	find_stack_bounds(rlist, &opt->stack_rstart, &opt->stack_rend);
}

/*
 * Calculate the current virtual memory address for stack values:
 * addr = stack_end - addr         This is x86 specific.
 * Look for type.on_stack, then clear it to handle those values as
 * regular static memory values. As long as type.on_stack is set,
 * those aren't processed.
 */

static inline void process_stack_end (Options *opt, list<CfgEntry> *cfg)
{
	list<CfgEntry>::iterator cfg_it;
	CfgEntry *cfg_en;
	list<CheckEntry> *chk_lp;
	list<CheckEntry>::iterator chk_it;
	CheckEntry *chk_en;

	ptr_t stack_end = opt->stack_end;

	if (!opt->val_on_stack)
		return;

	list_for_each (cfg, cfg_it) {
		cfg_en = &(*cfg_it);
		if (cfg_en->dynmem || cfg_en->ptrmem)
			continue;
		if (cfg_en->type.on_stack) {
			cfg_en->addr = stack_end - cfg_en->addr;
			// change the cache address only once
			if (!cfg_en->cache->is_dirty) {
				cfg_en->cache->is_dirty = true;
				cfg_en->cache->offs = stack_end - cfg_en->cache->offs;
			}
			cfg_en->type.on_stack = false;
		}

		chk_lp = cfg_en->checks;
		if (!chk_lp)
			continue;
		list_for_each (chk_lp, chk_it) {
			chk_en = &(*chk_it);
			if (!chk_en->type.on_stack)
				continue;
			chk_en->addr = stack_end - chk_en->addr;
			// change the cache address only once
			if (!chk_en->cache->is_dirty) {
				chk_en->cache->is_dirty = true;
				chk_en->cache->offs = stack_end - chk_en->cache->offs;
			}
			chk_en->type.on_stack = false;
		}
	}

	// clear dirty flags again
	list<CacheEntry>::iterator cait;
	list_for_each (opt->stack->cache_list, cait)
		cait->is_dirty = false;

	// HACK: Wait for the game to fill the stack with permanent data.
	sleep_sec(1);
}

// sf() callback for read_dynmem_buf()
void verify_stack_end (SF_PARAMS)
{
	struct dynmem_params *dmparams = (struct dynmem_params *) argp;
	struct sf_params *sfp = dmparams->sfparams;
	Options *opt = sfp->opt;

	if (opt->stack_end == stack_end) {
		ugout << "stack_end verified" << endl;
	} else if (!opt->stack_end) {
		opt->stack_end = stack_end;
		ugout << "stack_end: 0x" << hex << stack_end << dec << endl;
		process_stack_end(opt, cfg);
	} else {
		ugerr << "Fatal: stack_end 0x" << hex << stack_end
		      << " does not match 0x" << opt->stack_end
		      << " from /proc/$pid/stat" << dec << endl;
		exit(-1);
	}
}

void handle_stack_end (Options *opt, list<CfgEntry> *cfg, pid_t pid,
		       struct list_head *rlist)
{
	find_stack_bounds(rlist, &opt->stack_rstart, &opt->stack_rend);
	opt->stack_end = get_stack_end(pid, opt->stack_rstart, opt->stack_rend);
	if (opt->stack_end == opt->stack_rstart)
		opt->stack_end = 0;

	if (opt->stack_end) {
		ugout << "stack_end: 0x" << hex << opt->stack_end << dec << endl;
		process_stack_end(opt, cfg);
	} else if (opt->pure_statmem && opt->val_on_stack) {
		ugerr << "Fatal: Could not determine stack end." << endl;
		exit(-1);
	}
}
