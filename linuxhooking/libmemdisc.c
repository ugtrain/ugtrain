/* libmemdisc.c:    discovery of an unique malloc call
 *
 * Copyright (c) 2013, by:      Sebastian Riemer
 *    All rights reserved.     <sebastian.riemer@gmx.de>
 *
 * powered by the Open Game Cheating Association
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 2, or any later version
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

#define _GNU_SOURCE
#include <dlfcn.h>      /* dlsym */
#include <stdio.h>      /* printf */
#include <stdlib.h>     /* malloc */
#include <string.h>
#include <fcntl.h>
#include <signal.h>     /* sigignore */
#include <unistd.h>     /* read */
#include <limits.h>     /* PIPE_BUF */
#include <execinfo.h>   /* backtrace */
#include "../src/common.h"

#define PFX "[memdisc] "
#define OW_MALLOC 1
#define OW_FREE 1
#define BUF_SIZE PIPE_BUF/2
#define DYNMEM_IN  "/tmp/memhack_in"
#define DYNMEM_OUT "/tmp/memhack_out"
#define MAX_BT 11   /* for reverse stack search only */

#define DEBUG 0
#if !DEBUG
	#define printf(...) do { } while (0);
#endif

/*
 * Ask gcc for the current stack frame pointer.
 * We don't use the stack pointer as we are not interested in the
 * stuff we have ourselves on the stack and for arch independence.
 */
#ifndef FIRST_FRAME_POINTER
	# define FIRST_FRAME_POINTER  __builtin_frame_address (0)
#endif


/* File descriptors and output buffer */
static i32 ifd = -1;
static FILE *ofile = NULL;  /* much data - we need caching */

/* Output control */
static bool active = false;
static i32 stage = 0;  /* 0: no output */

/* Input parameters */
/* Output filtering */

/* relevant start and end memory addresses on the heap */
static void *heap_saddr = NULL, *heap_eaddr = NULL;

/* malloc size */
static size_t malloc_size = 0;

/* Backtracing */

/* This is a global variable set at program start time. It marks the
   greatest used stack address. */
extern void *__libc_stack_end;

/* relevant start and end code addresses of .text segment */
static void *bt_saddr = NULL, *bt_eaddr = NULL;

/* code address of the interesting malloc call */
static void *code_addr = NULL;

/*
 * ATTENTION: GNU backtrace() might crash with SIGSEGV!
 *
 * So use it if explicitly requested only.
 * If not, we proceed with reverse searching for code
 * addresses on the stack without respecting further stack frames.
 */
static bool use_gbt = false;


#define READ_STAGE_CFG()  \
	rbytes = read(ifd, ibuf, sizeof(ibuf)); \
	if (rbytes <= 0) { \
		fprintf(stderr, PFX "Can't read config for stage %c, " \
			"disabling output.\n", ibuf[0]); \
		return; \
	}

/* prepare memory hacking on library load */
void __attribute ((constructor)) memdisc_init (void)
{
	ssize_t rbytes;
	i32 read_tries, ioffs = 0;
	char ibuf[128] = { 0 };
	char gbt_buf[sizeof(GBT_CMD)] = { 0 };
	void *heap_start = NULL, *heap_soffs = NULL, *heap_eoffs = NULL;

	sigignore(SIGPIPE);
	sigignore(SIGCHLD);

	fprintf(stdout, PFX "Stack end:  %p\n", __libc_stack_end);
	/*
	 * Don't you dare call malloc for another purpose in this lib!
	 * We can only do this safely as (active == false).
	 */
	heap_start = malloc(1);
	if (heap_start) {
		fprintf(stdout, PFX "Heap start: %p\n", heap_start);
		heap_saddr = heap_eaddr = heap_start;
		free(heap_start);
	}

	ifd = open(DYNMEM_IN, O_RDONLY | O_NONBLOCK);
	if (ifd < 0) {
		perror(PFX "open input");
		exit(1);
	}
	printf(PFX "ifd: %d\n", ifd);

	printf(PFX "Waiting for output FIFO opener..\n");
	ofile = fopen(DYNMEM_OUT, "w");
	if (!ofile) {
		perror(PFX "fopen output");
		exit(1);
	}
	printf(PFX "ofile: %p\n", ofile);

	for (read_tries = 5; ; --read_tries) {
		rbytes = read(ifd, ibuf, 2);
		if (rbytes > 0)
			break;
		if (read_tries <= 0)
			goto read_err;
		usleep(250 * 1000);
	}

	fprintf(ofile, "h%p\n", heap_start);

	switch (ibuf[0]) {
	/*
	 * stage 1: Find malloc size  (together with static memory search)
	 *	There are lots of mallocs and frees - we need to filter the
	 *	output for a distinct memory area (on the heap) determined
	 *	by static memory search. The interesting bit is a malloc
	 *	without a free where (mem_addr <= found_addr < mem_addr+size).
	 */
	case '1':
		READ_STAGE_CFG();
		if (sscanf(ibuf, "%p;%p", &heap_soffs, &heap_eoffs) == 2) {
			heap_saddr = PTR_ADD(void *, heap_saddr, heap_soffs);
			heap_eaddr = PTR_ADD(void *, heap_eaddr, heap_eoffs);
			stage = 1;
			active = true;
		} else {
			goto parse_err;
		}
		break;
	/*
	 * stage 2: Verify malloc size
	 *	If we are lucky, the found malloc size is a rare value in the
	 *	selected memory area. So we shouldn't find it too often. We
	 *	don't want to see the frees here anymore. Repeating this step
	 *	also shows us if our heap filters are always applicable.
	 *
	 *	With the malloc size 0, this can also be used like stage 1 but
	 *	with ignoring the frees.
	 */
	case '2':
		READ_STAGE_CFG();
		if (sscanf(ibuf, "%p;%p;%zd", &heap_soffs, &heap_eoffs,
		    &malloc_size) == 3) {
			heap_saddr = PTR_ADD(void *, heap_saddr, heap_soffs);
			heap_eaddr = PTR_ADD(void *, heap_eaddr, heap_eoffs);
			stage = 2;
			active = true;
		} else {
			goto parse_err;
		}
		break;
	/*
	 * stage 3: Get the code address  (by backtracing)
	 *	By default we search the stack memory reverse for code
	 *	addresses. While doing so we don't respect stack frames in
	 *	contrast to what GNU backtrace does to be less error prone.
	 *	But the downside is that we find a lot of false positives.
	 *
	 *	GNU backtrace is better suited for automated adaption. If
	 *	it works here without crashing with SIGSEGV, then it works
	 *	in libmemhack as well and stage 4 is not required anymore.
	 *	Insert 'gbt;' after '3;' to activate it.
	 *
	 *	You need to disassemble the victim binary to get the
	 *	code address area which is within the .text segment.
	 *	With that we can ignore invalid code addresses.
	 */
	case '3':
		READ_STAGE_CFG();
		if (sscanf(ibuf, "%3s;", gbt_buf) == 1 &&
		    strncmp(gbt_buf, GBT_CMD, sizeof(GBT_CMD) - 1) == 0) {
			use_gbt = true;
			ioffs = sizeof(GBT_CMD);
		}
		if (sscanf(ibuf + ioffs, "%p;%p;%zd;%p;%p", &heap_soffs,
		    &heap_eoffs, &malloc_size, &bt_saddr, &bt_eaddr) == 5) {
			heap_saddr = PTR_ADD(void *, heap_saddr, heap_soffs);
			heap_eaddr = PTR_ADD(void *, heap_eaddr, heap_eoffs);
			if (malloc_size < 1)
				use_gbt = false;
			if (use_gbt)
				fprintf(stdout, PFX "Using GNU backtrace(). "
					"This might crash with SIGSEGV!\n");
			stage = 3;
			active = true;
		} else {
			goto parse_err;
		}
		break;
	/*
	 * stage 4/5: Get the reverse stack offset  (not for GNU backtrace)
	 *	We can use this stage directly and skip stage 3 if we aren't
	 *	using GNU backtrace. Reverse stack offsets are determined
	 *	relative to the current stack frame pointer. The advantage of
	 *	knowing the reverse stack offset is that we can directly check
	 *	in libmemhack if the code address is at this location which
	 *	gives us better performance and stability. But the downside is
	 *	that we have to do one more step to discover and adapt them.
	 *
	 *	The difference between the stages 4 and 5 can only be found in
	 *	ugtrain. Stage 5 is used for the automatic adaption there
	 *	instead of initial discovery. For successful adaption we need
	 *	to trigger allocation of at least one memory object per class
	 *	in the game.
	 */
	case '4':
	case '5':
		READ_STAGE_CFG();
		if (sscanf(ibuf, "%p;%p;%zd;%p;%p;%p", &heap_soffs, &heap_eoffs,
		    &malloc_size, &bt_saddr, &bt_eaddr, &code_addr) >= 5) {
			heap_saddr = PTR_ADD(void *, heap_saddr, heap_soffs);
			heap_eaddr = PTR_ADD(void *, heap_eaddr, heap_eoffs);
			stage = 4;
			active = true;
		} else {
			goto parse_err;
		}
		break;
	/* stage 0: static memory search: do nothing */
	default:
		break;
	}

	if (heap_eaddr <= heap_saddr)
		heap_eaddr = (void *) -1UL;

	return;
read_err:
	fprintf(stderr, PFX "Can't read config, disabling output.\n");
	return;
parse_err:
	fprintf(stderr, PFX "Error while discovery input parsing! Ignored.\n");
	return;
}

#if DEBUG && 0
/* debugging for stack backtracing */
static void dump_stack_raw (void *ffp)
{
	void *offs;
	i32 col = 0;
	i32 byte;

	printf("\n");
	for (offs = ffp; offs < __libc_stack_end; offs++) {
		if (col >= 16) {
			printf("\n");
			col = 0;
		} else if (col == 8) {
			printf(" ");
		}
		if (col == 0)
			printf("%p: ", offs);
		byte = *(char *) offs;
		printf(" %02x", byte & 0xFF);
		col++;
	}
	printf("\n\n");
}
#else
static void dump_stack_raw (void* ffp) {}
#endif

/*
 * Backtrace by searching for code addresses on the stack without respecting
 * stack frames in contrast to GNU backtrace. If GNU backtrace hits NULL
 * pointers while determining the stack frames, then it crashes with SIGSEGV.
 *
 * We expect the first frame pointer to be (32/64 bit) memory aligned here.
 */
static bool find_code_pointers (void *ffp, char *obuf, i32 obuf_offs)
{
	void *offs, *code_ptr;
	i32 i = 0;
	bool found = false;

	printf(PFX "ffp: %p\n", ffp);
	for (offs = ffp;
	     offs <= __libc_stack_end - sizeof(void *);
	     offs += sizeof(void *)) {
		code_ptr = (void *) *(ptr_t *) offs;
		if (code_ptr >= bt_saddr && code_ptr <= bt_eaddr) {
			if (stage == 4 &&
			    (code_addr == NULL ||
			     code_ptr == code_addr)) {
				obuf_offs += sprintf(obuf + obuf_offs, ";c%p;o%p",
					code_ptr,
					(void *) (offs - ffp));
				found = true;
			} else if (stage == 3) {
				obuf_offs += sprintf(obuf + obuf_offs, ";c%p",
					code_ptr);
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
static bool run_gnu_backtrace (char *obuf, i32 obuf_offs)
{
	bool found = false;
	void *trace[MAX_GNUBT] = { NULL };
	i32 i, num_taddr = 0;

	num_taddr = backtrace(trace, MAX_GNUBT);
	if (num_taddr > 1) {
		/* skip the first code addr (our own one) */
		for (i = 1; i < num_taddr; i++) {
			if (trace[i] >= bt_saddr && trace[i] <= bt_eaddr) {
				obuf_offs += sprintf(obuf + obuf_offs, ";c%p",
					trace[i]);
				found = true;
			}
		}
	}
	return found;
}

/* void *malloc (size_t size); */
/* void *calloc (size_t nmemb, size_t size); */
/* void *realloc (void *ptr, size_t size); */
/* void free (void *ptr); */

#ifdef OW_MALLOC
void *malloc (size_t size)
{
	void *ffp = FIRST_FRAME_POINTER;
	void *mem_addr;
	i32 wbytes;
	char obuf[BUF_SIZE];
	i32 obuf_offs = 0;
	bool found;
	static void *(*orig_malloc)(size_t size) = NULL;

	/* get the libc malloc function */
	if (!orig_malloc)
		orig_malloc = (void *(*)(size_t)) dlsym(RTLD_NEXT, "malloc");

	mem_addr = orig_malloc(size);

	if (active && mem_addr > heap_saddr && mem_addr < heap_eaddr) {
		if (malloc_size > 0 && size != malloc_size)
			goto out;
		obuf_offs = sprintf(obuf, "m%p;s%zd", mem_addr, size);

		if (stage >= 3) {
			dump_stack_raw(ffp);  /* debugging only */
			if (use_gbt)
				found = run_gnu_backtrace(obuf, obuf_offs);
			else
				found = find_code_pointers(ffp, obuf, obuf_offs);
			if (!found)
				goto out;
		}
		strcat(obuf, "\n");

		wbytes = fprintf(ofile, "%s", obuf);
		if (wbytes < 0) {
			//perror(PFX "fprintf");
			//exit(1);
		}
	}
out:
	return mem_addr;
}
#endif


#ifdef OW_FREE
void free (void *ptr)
{
	i32 wbytes;
	char obuf[BUF_SIZE];
	static void (*orig_free)(void *ptr) = NULL;

	if (active && ptr > heap_saddr && ptr < heap_eaddr) {
		if (stage > 1)
			goto out;
		sprintf(obuf, "f%p\n", ptr);

		wbytes = fprintf(ofile, "%s", obuf);
		if (wbytes < 0) {
			//perror(PFX "fprintf");
			//exit(1);
		}
	}
out:
	/* get the libc free function */
	if (!orig_free)
		orig_free = (void (*)(void *)) dlsym(RTLD_NEXT, "free");

	orig_free(ptr);
}
#endif
