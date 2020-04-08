/* libmemdisc.c:    discovery of a unique malloc call
 *
 * Copyright (c) 2012..2018 Sebastian Parschauer <s.parschauer@gmx.de>
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>      /* printf */
#include <stdlib.h>     /* malloc */
#include <string.h>     /* strchr */
#include <ctype.h>      /* isalpha */
#include <libgen.h>     /* basename */
#include <fcntl.h>      /* open */
#include <signal.h>     /* sigignore */
#include <unistd.h>     /* read */
#include <limits.h>     /* PIPE_BUF */
#include <execinfo.h>   /* backtrace */

#include <lib/system.h>
#include "libcommon.h"

#define PFX "[memdisc] "
#define DYNMEM_IN   "/tmp/memhack_in"
#define DYNMEM_OUT  "/tmp/memhack_out"
#define MEMDISC_OUT "/tmp/memdisc_out"
#define MAX_BT 11		/* for reverse stack search only */


/* File descriptors and output buffer */
static i32 ifd = -1, ofd = -1;
static FILE *ofile = NULL;  /* much data - we need caching */

/* Output control */
static bool active = false;
static bool discover_ptr = false;
static i32 stage = 0;  /* 0: no output */

/* Input parameters */
/* Output filtering */

/* malloc size */
static size_t malloc_size = 0;

/* Backtracing */

/* This is a global variable set at program start time. It marks the
   greatest used stack address. */
extern void *__libc_stack_end;
#define stack_end (ptr_t) __libc_stack_end

/* ask libc for our process name as 'argv[0]' and 'getenv("_")'
   don't work here */
extern char *__progname;

/* filter discovery backtrace output to code from the exe or this lib only */
static char *bt_filter;

/* relevant start and end code addresses of .text segment */
static ptr_t bt_saddr = 0, bt_eaddr = 0;

/* code address of the interesting malloc call */
static ptr_t orig_code_addr = 0, code_addr = 0;

/*
 * ATTENTION: GNU backtrace() might crash with SIGSEGV!
 *
 * So use it if explicitly requested only.
 * If not, we proceed with reverse searching for code
 * addresses on the stack without respecting further stack frames.
 */
static bool use_gbt = false;

/* Config structure for pointer to heap object discovery */
struct cfg {
	size_t mem_size;
	ptr_t code_addr;
	ptr_t stack_offs;
	ptr_t mem_addr;   /* filled by malloc */
	ptr_t ptr_offs;
};
typedef struct cfg cfg_s;

static cfg_s ptr_cfg;


static inline void update_cfg_for_pie (ptr_t exe_offs)
{
	/* pointer to heap obj. discovery PIE handling */
	if (ptr_cfg.code_addr &&
	    ptr_cfg.code_addr <= UINTPTR_MAX - exe_offs) {
		ptr_cfg.code_addr += exe_offs;
		pr_dbg("new ptr cfg code addr: " PRI_PTR "\n",
			ptr_cfg.code_addr);
	} else if (ptr_cfg.ptr_offs &&
	    ptr_cfg.ptr_offs <= UINTPTR_MAX - exe_offs) {
		ptr_cfg.ptr_offs += exe_offs;
		pr_dbg("new ptr cfg ptr addr: " PRI_PTR "\n",
			ptr_cfg.ptr_offs);
	}
	/* stage >= 3 PIE handling */
	if (code_addr && code_addr <= UINTPTR_MAX - exe_offs)
		code_addr += exe_offs;
}

/*
 * Does the user request to filter the backtrace to the exe or the given lib?
 * returns 0 for success, -1 for error
 */
static inline i32 parse_bt_filter (char **iptr)
{
	i32 ret = -1;
	u32 len;
	char *pos;
	char *disc_part = *iptr;

	if (disc_part[0] != ';' || !isalpha(disc_part[1]))
		goto success;

	disc_part++;
	pos = strchr(disc_part, ';');
	if (!pos)
		goto out;
	len = pos - disc_part;
	*pos = '\0';
	if (len == 3 && strcmp(disc_part, "exe") == 0) {
		bt_filter = (char *) "\0";
		pr_out("PIE: filtering backtrace to executable\n");
	} else {
		bt_filter = (char *) malloc(len + 1);
		memcpy(bt_filter, disc_part, len);
		bt_filter[len] = '\0';
		pr_out("PIC: filtering backtrace to %s\n", bt_filter);
	}
	*pos = ';';
	disc_part = pos;
	*iptr = disc_part;
success:
	ret = 0;
out:
	return ret;
}

#define READ_STAGE_CFG()  \
	rbytes = read(ifd, iptr, buf_left); \
	if (rbytes <= 0) { \
		pr_err("Can't read config for stage %c, " \
			"disabling output.\n", ibuf[0]); \
		return; \
	} \
	buf_left -= rbytes; \
	*(iptr + rbytes) = '\0';

/*
 * To be registered with atexit() to flush the output FIFO cache.
 *
 * The flush is especially needed if there is only a
 * small amount of data in the FIFO cache which would
 * never be written otherwise. We only need to do it
 * once upon exit and we do it with atexit() as it
 * doesn't work in the library destructor.
 */
static void flush_output (void)
{
	fflush(ofile);
}

/*
 * Assumption: ifd >= 0
 */
static inline i32 read_input (char ibuf[], size_t size)
{
	i32 ret = -1;
	i32 read_tries;
	ssize_t rbytes;

	for (read_tries = 5; ; --read_tries) {
		rbytes = read(ifd, ibuf, size);
		if (rbytes > 0) {
			ibuf[rbytes] = '\0';
			ret = 0;
			break;
		}
		if (read_tries <= 0)
			break;
		sleep_msec_unless_input(250, ifd);
	}
	return ret;
}

/*
 * PIC handling: read the backtrace filter from the input FIFO
 *
 * Attention: Might be called multiple times for the same library.
 */
static inline void set_bt_addrs (void)
{
	ptr_t lib_start, lib_end;
	char ibuf[128] = { 0 };
	i32 ret;

	if (read_input(ibuf, sizeof(ibuf) - 1) != 0) {
		pr_err("Couldn't read library backtrace filter!\n");
		goto out;
	}
	ret = sscanf(ibuf, SCN_PTR ";" SCN_PTR, &lib_start, &lib_end);
	if (ret < 2) {
		pr_err("Library backtrace filter parsing error!\n");
		goto out;
	}
	bt_saddr = lib_start;
	bt_eaddr = lib_end;

	/* stage >= 3 PIC handling */
	if (code_addr && code_addr <= UINTPTR_MAX - lib_start) {
		if (!orig_code_addr)
			orig_code_addr = code_addr;
		code_addr = orig_code_addr + lib_start;
	}

	pr_out("bt_filter: %s ("PRI_PTR "-" PRI_PTR")\n", bt_filter,
		bt_saddr, bt_eaddr);
out:
	return;
}

/* clean up upon failure in library constructor */
void do_cleanup (void)
{
	if (ifd >= 0) {
		close(ifd);
		ifd = -1;
	}
	if (ofile) {
		fflush(ofile);
		fclose(ofile);
		ofile = NULL;
	}
#if USE_DEBUG_LOG
	if (DBG_FILE_VAR) {
		fflush(DBG_FILE_VAR);
		fclose(DBG_FILE_VAR);
		DBG_FILE_VAR = NULL;
	}
#endif
}

/* prepare memory discovery upon library load */
void __attribute ((constructor)) memdisc_init (void)
{
	char *proc_name = NULL, *expected = NULL;
	ssize_t rbytes;
	i32 scanned;
	char ibuf[BUF_SIZE] = { 0 };
	char *iptr = ibuf, *pos;
	size_t buf_left = sizeof(ibuf) - 1;
	char gbt_buf[sizeof(GBT_CMD)] = { 0 };

#if USE_DEBUG_LOG
	if (!DBG_FILE_VAR) {
		DBG_FILE_VAR = fopen(DBG_FILE_NAME, "a+");
		if (!DBG_FILE_VAR) {
			perror(PFX "fopen debug log");
			exit(1);
		}
	}
#endif
	/* only care for the game process (ignore shell and others) */
	expected = secure_getenv(UGT_GAME_PROC_NAME);
	if (expected) {
		proc_name = __progname;
		pr_dbg("proc_name: %s, exp: %s\n", proc_name, expected);
		if (strcmp(expected, proc_name) != 0)
			return;
	}

	/* We are preloaded into the right process - stop preloading us!
	   This also hides our presence from the game. ;-) */
	rm_from_env(PRELOAD_VAR, "libmemdisc", ':');

	sigignore(SIGPIPE);
	sigignore(SIGCHLD);

	if (active)
		return;

	pr_out("Stack end:  %p\n", __libc_stack_end);

	if (ifd >= 0)
		goto out;
	ifd = open(DYNMEM_IN, O_RDONLY | O_NONBLOCK);
	if (ifd < 0) {
		perror(PFX "open input");
		exit(1);
	}
	pr_dbg("ifd: %d\n", ifd);

	if (ofile)
		goto out;
	pr_out("Waiting for output FIFO opener..\n");
	ofile = fopen(MEMDISC_OUT, "w");
	if (!ofile) {
		perror(PFX "fopen discovery output");
		exit(1);
	}
	pr_dbg("ofile: %p\n", ofile);

	if (ofd >= 0)
		goto out;
	ofd = open(DYNMEM_OUT, O_WRONLY);
	if (ofd < 0) {
		perror(PFX "open output");
		exit(1);
	}
	pr_dbg("ofd: %d\n", ifd);

	if (read_input(ibuf, 1) != 0)
		goto read_err;
	buf_left--;
	iptr++;

	memset(&ptr_cfg, 0, sizeof(ptr_cfg));
	if (ibuf[0] == 'p') {
		READ_STAGE_CFG();
		if (sscanf(iptr, ";%zd;" SCN_PTR ";" SCN_PTR ";"
		    SCN_PTR, &ptr_cfg.mem_size, &ptr_cfg.code_addr,
		    &ptr_cfg.stack_offs, &ptr_cfg.ptr_offs) != 4)
			goto parse_err;
		pos = strstr(iptr, ";;");
		if (!pos)
			goto parse_err;
		iptr = pos + 2;
		discover_ptr = true;

		pr_dbg("ibuf: %s\n", ibuf);
		pr_dbg("iptr; %p\n", iptr);
		pr_dbg("ptr_cfg: %zd;" PRI_PTR ";" PRI_PTR ";" PRI_PTR "\n",
			ptr_cfg.mem_size, ptr_cfg.code_addr,
			ptr_cfg.stack_offs, ptr_cfg.ptr_offs);
	} else if (ibuf[0] >= '1' && ibuf[0] <= '4') {
		READ_STAGE_CFG();
		iptr = ibuf;
	}

	switch (*iptr) {
	/*
	 * stage 1: Find the malloc size  (with memory search in parallel)
	 *
	 *	There are lots of mallocs and frees - we need to filter the
	 *	output for a distinct memory object on the heap. Memory search
	 *	provides a heap memory address on a variable within the object.
	 *	That is used for finding the matching memory object.
	 *	The interesting bit is the last matching malloc in that area
	 *	where (mem_addr <= found_addr < mem_addr+size).
	 *
	 *	This is rarely used as the frees aren't required.
	 */
	case '1':
		iptr++;
		stage = 1;
		pr_dbg("stage 1 selected\n");
		break;
	/*
	 * stage 2: Verify the malloc size
	 *
	 *	If we are lucky, the found malloc size is a rare value on the
	 *	heap. So we shouldn't find it too often. We don't want to see
	 *	the frees here.
	 *
	 *	With the malloc size 0, this can also be used like stage 1 but
	 *	with ignoring the frees.
	 */
	case '2':
		iptr++;
		scanned = sscanf(iptr, ";%zd", &malloc_size);
		if (scanned < 0)
			goto parse_err;
		stage = 2;
		pr_dbg("stage 2 cfg: %zd\n", malloc_size);
		break;
	/*
	 * stage 3: Get the code address  (by backtracing)
	 *
	 *	By default we search the stack memory reverse for code
	 *	addresses. While doing so we don't respect stack frames in
	 *	contrast to what GNU backtrace does to be less error prone.
	 *	But the downside is that we might find some false positives.
	 *
	 *	GNU backtrace would be better suited for automated adaptation,
	 *	can be also used with libmemhack but it is unreliable as it
	 *	requires the stack frames and often crashes with SIGSEGV. It
	 *	is also slower. So use it for testing only.
	 *	Insert 'gbt;' behind '3;' to activate it.
	 *
	 *	Ugtrain reads from /proc/$pid/maps to filter out invalid code
	 *	addresses.
	 */
	case '3':
		iptr++;
		if (sscanf(iptr, ";%3s;", gbt_buf) == 1 &&
		    strncmp(gbt_buf, GBT_CMD, sizeof(GBT_CMD) - 1) == 0) {
			use_gbt = true;
			iptr += sizeof(GBT_CMD);
		}
		if (parse_bt_filter(&iptr))
			goto parse_err;
		scanned = sscanf(iptr, ";%zd", &malloc_size);
		if (scanned < 1)
			goto parse_err;
		if (malloc_size < 1)
			use_gbt = false;
		if (use_gbt)
			pr_out("Using GNU backtrace(). "
				"This might crash with SIGSEGV!\n");
		stage = 3;
		pr_dbg("stage 3 cfg: %zd\n", malloc_size);
		break;
	/*
	 * stage 4: Get the reverse stack offset  (not for GNU backtrace)
	 *
	 *	We can use this stage directly and skip stage 3 if we aren't
	 *	using GNU backtrace. Reverse stack offsets are determined
	 *	relative to the current stack frame pointer. The advantage of
	 *	knowing the reverse stack offset is that we can directly check
	 *	in libmemhack if the code address is at this location which
	 *	gives us better performance and stability.
	 */
	case '4':
		iptr++;
		if (parse_bt_filter(&iptr))
			goto parse_err;
		scanned = sscanf(iptr, ";%zd;" SCN_PTR, &malloc_size,
			&code_addr);
		if (scanned < 1)
			goto parse_err;
		stage = 4;
		pr_dbg("stage 4 cfg: %zd;" PRI_PTR "\n", malloc_size,
			code_addr);
		break;
	/* stage 0: static memory search: do nothing */
	default:
		goto stage_unknown;
	}

	/* Register final output flushing */
	atexit(flush_output);

	if (!code_addr && bt_eaddr <= bt_saddr)
		bt_eaddr = UINTPTR_MAX;

	/* Read new backtrace filter config (might be PIC/PIE) */
	if (stage >= 3 || ptr_cfg.code_addr != 0) {
		/* notify ugtrain to do the PIE handling */
		ssize_t wbytes;
		wbytes = write(ofd, ASLR_NOTIFY_STR, sizeof(ASLR_NOTIFY_STR));
		if (wbytes != sizeof(ASLR_NOTIFY_STR)) {
			pr_err("PIE handling: Can't write notification.\n");
			goto out;
		}
		/* PIE handling if exe filter is requested */
		if (bt_filter && bt_filter[0] == '\0') {
			ptr_t exe_start = 0, exe_end = 0, exe_offs = 0;
			i32 ret;
			if (read_input(ibuf, sizeof(ibuf) - 1) != 0) {
				pr_err("Couldn't read exe backtrace filter!\n");
				goto out;
			}
			ret = sscanf(ibuf, SCN_PTR ";" SCN_PTR ";" SCN_PTR,
				&exe_start, &exe_end, &exe_offs);
			if (ret < 3) {
				pr_err("Exe backtrace filter parsing error!\n");
				goto out;
			}
			bt_saddr = exe_start;
			bt_eaddr = exe_end;
			update_cfg_for_pie(exe_offs);

			pr_out("bt_filter: exe ("PRI_PTR "-" PRI_PTR")\n",
				exe_start, exe_end);
		/* PIC handling if library filter is requested */
		} else if (bt_filter && bt_filter[0] != '\0') {
			/* get early loaded lib backtrace filter or just delay
			   execution until ugtrain has PIC handling in place */
			set_bt_addrs();
		}
	}
	pr_dbg("new cfg: %d;%zd;" PRI_PTR "\n", stage, malloc_size, code_addr);

	/* Send out the stack end */
	fprintf(ofile, "S" PRI_PTR "\n", stack_end);
	active = true;
out:
	/*
	 * don't need the input FIFO any more
	 * (unless library backtrace filter set)
	 */
	if (ifd >= 0 && (!bt_filter || bt_filter[0] == '\0')) {
		close(ifd);
		ifd = -1;
	}
	return;
read_err:
	pr_err("Can't read config! Staying inactive.\n");
	do_cleanup();
	return;
parse_err:
	pr_err("Error while discovery input parsing! Staying inactive.\n");
	do_cleanup();
	return;
stage_unknown:
	do_cleanup();
	return;
}

/*
 * Get a specific pointer value, pointing to a heap address:
 * 1. from within another heap object
 * 2. from a static memory address
 *
 * Assumption: (discover_ptr == true)
 */
static void get_ptr_to_heap (size_t size, ptr_t mem_addr, ptr_t ffp,
			     char *obuf, i32 *obuf_offs)
{
	ptr_t stack_addr, ptr_addr = 0;
	static ptr_t old_ptr_addr = 0;

	if (ptr_cfg.code_addr) {
		if (size == ptr_cfg.mem_size) {
			stack_addr = ffp + ptr_cfg.stack_offs;
			if (stack_addr <= stack_end - sizeof(ptr_t) &&
			    *(ptr_t *) stack_addr == ptr_cfg.code_addr)
				ptr_cfg.mem_addr = mem_addr;
		}
		if (ptr_cfg.mem_addr) {
			ptr_addr = ptr_cfg.mem_addr + ptr_cfg.ptr_offs;
			ptr_addr = *(ptr_t *) ptr_addr;
		}
	} else if (ptr_cfg.ptr_offs) {
		ptr_addr = *(ptr_t *) ptr_cfg.ptr_offs;
	}
	if (ptr_addr && ptr_addr != old_ptr_addr) {
		i32 wbytes = snprintf(obuf + *obuf_offs, BUF_SIZE - *obuf_offs,
				  "p" PRI_PTR "\n", ptr_addr);
		if (wbytes < 0)
			perror(PFX "snprintf");
		else
			*obuf_offs += wbytes;
		old_ptr_addr = ptr_addr;
	}
}

#if DEBUG_MEM && 0
/* debugging for stack backtracing */
static void dump_stack_raw (ptr_t ffp)
{
	ptr_t offs;
	i32 col = 0;
	i32 byte;

	printf("\n");
	for (offs = ffp; offs < stack_end; offs++) {
		if (col >= 16) {
			printf("\n");
			col = 0;
		} else if (col == 8) {
			printf(" ");
		}
		if (col == 0)
			printf(PRI_PTR ": ", offs);
		byte = *(char *) offs;
		printf(" %02x", byte & 0xFF);
		col++;
	}
	printf("\n\n");
}
#else
static void dump_stack_raw (ptr_t ffp) {}
#endif

/*
 * Backtrace by searching for code addresses on the stack without respecting
 * stack frames in contrast to GNU backtrace. If GNU backtrace hits NULL
 * pointers while determining the stack frames, then it crashes with SIGSEGV.
 *
 * We expect the first frame pointer to be (32/64 bit) memory aligned here.
 */
static inline bool find_code_addresses (ptr_t ffp, char *obuf, i32 *obuf_offs)
{
	ptr_t offs, code_addr_os;  /* stack offset and code address on stack */
	i32 i = 0;
	bool found = false;

	if (!ffp)
		return false;

	for (offs = ffp;
	     offs <= stack_end - sizeof(ptr_t);
	     offs += sizeof(ptr_t)) {
		code_addr_os = *(ptr_t *) offs;
		if (code_addr_os >= bt_saddr && code_addr_os <= bt_eaddr) {
			if (stage == 4 &&
			    (!code_addr ||
			     code_addr_os == code_addr)) {
				i32 wbytes = snprintf(obuf + *obuf_offs,
					BUF_SIZE - *obuf_offs,
					";c" PRI_PTR ";o" PRI_PTR,
					code_addr_os, offs - ffp);
				if (wbytes < 0)
					perror(PFX "snprintf");
				else
					*obuf_offs += wbytes;
				found = true;
			} else if (stage == 3) {
				i32 wbytes = snprintf(obuf + *obuf_offs,
					BUF_SIZE - *obuf_offs,
					";c" PRI_PTR, code_addr_os);
				if (wbytes < 0)
					perror(PFX "snprintf");
				else
					*obuf_offs += wbytes;
				found = true;
			}
			i++;
			if (i >= MAX_BT)
				break;
		}
	}
	return found;
}

/* ATTENTION: GNU backtrace() might crash with SIGSEGV! */
static inline bool run_gnu_backtrace (char *obuf, i32 *obuf_offs)
{
	bool found = false;
	void *trace[MAX_GNUBT] = { NULL };
	i32 i, num_taddr = 0;

	num_taddr = backtrace(trace, MAX_GNUBT);
	if (num_taddr > 1) {
		/* skip the first code addr (our own one) */
		for (i = 1; i < num_taddr; i++) {
			ptr_t trace_addr = (ptr_t) trace[i];
			if (trace_addr >= bt_saddr && trace_addr <= bt_eaddr) {
				i32 wbytes = snprintf(obuf + *obuf_offs,
					BUF_SIZE - *obuf_offs,
					";c" PRI_PTR, trace_addr);
				if (wbytes < 0)
					perror(PFX "snprintf");
				else
					*obuf_offs += wbytes;
				found = true;
			}
		}
	}
	return found;
}

static inline void write_obuf (char obuf[])
{
	i32 wbytes;

	if (!ofile)
		return;
#if DEBUG_MEM
	pr_out("%s", obuf);
#endif
	wbytes = fputs(obuf, ofile);
	if (wbytes < 0) {
		perror(PFX "fputs");
		exit(1);
	}
}

/*
 * A real function call is required to unify the malloc hooks.
 * The unpreferred GNU backtrace() method sees the call to this function.
 */
void postprocess_malloc (ptr_t ffp, size_t size, ptr_t mem_addr)
{
	i32 wbytes;
	char obuf[BUF_SIZE + 1] = { 0 };
	i32 obuf_offs = 0;
	bool found;

	if (!active)
		goto out;

	if (size == 0 || (malloc_size > 0 && size != malloc_size &&
	    size != ptr_cfg.mem_size))
		goto out;
	wbytes = snprintf(obuf, BUF_SIZE, "m" PRI_PTR ";s%zd", mem_addr, size);
	if (wbytes < 0)
		perror(PFX "snprintf");
	else
		obuf_offs += wbytes;

	if (stage >= 3) {
		dump_stack_raw(ffp);  /* debugging only */
		if (use_gbt)
			found = run_gnu_backtrace(obuf, &obuf_offs);
		else
			found = find_code_addresses(ffp, obuf, &obuf_offs);
		if (!found) {
#if DEBUG_MEM
			pr_out("find_code_addresses() failed!\n");
#endif
			goto out;
		}
	}
	wbytes = snprintf(obuf + obuf_offs, BUF_SIZE - obuf_offs, "\n");
	if (wbytes < 0)
		perror(PFX "snprintf");
	else
		obuf_offs += wbytes;

	if (discover_ptr)
		get_ptr_to_heap(size, mem_addr, ffp, obuf, &obuf_offs);
	/* only send out terminated messages */
	if (obuf_offs >= 1 && obuf[obuf_offs - 1] == '\n')
		write_obuf(obuf);
	else
		pr_err("%s: not terminated message detected!\n", __func__);
out:
	return;
}

/*
 * Write the memory address to be freed to the output FIFO.
 */
void preprocess_free (ptr_t mem_addr)
{
	i32 wbytes;
	char obuf[BUF_SIZE + 1] = { 0 };

	if (!active || stage != 1)
		return;

	wbytes = snprintf(obuf, BUF_SIZE, "f" PRI_PTR "\n", mem_addr);
	if (wbytes < 0)
		perror(PFX "snprintf");
	write_obuf(obuf);
}

/*
 * Inform ugtrain about loading of a library (late PIC handling).
 */
void postprocess_dlopen (const char *lib_path)
{
	i32 wbytes;
	char obuf[BUF_SIZE + 1] = { 0 };
	char *lib_name;

	if (!active)
		return;

	lib_name = basename((char *) lib_path);
	wbytes = snprintf(obuf, BUF_SIZE, "l;%s\n", lib_name);
	if (wbytes < 0) {
		perror(PFX "snprintf");
		return;
	}
#if DEBUG_MEM
	pr_out("%s", obuf);
#endif
	wbytes = write(ofd, obuf, wbytes);
	if (wbytes < 0) {
		perror("dlopen write");
		return;
	}
	if (bt_filter && bt_filter[0] != '\0' && strstr(lib_name, bt_filter)) {
		pr_out("Reading backtrace filter for %s\n", lib_name);
		set_bt_addrs();
	}
}
