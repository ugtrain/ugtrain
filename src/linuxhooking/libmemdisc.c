/* libmemdisc.c:    discovery of an unique malloc call
 *
 * Copyright (c) 2012..14, by:  Sebastian Parschauer
 *    All rights reserved.     <s.parschauer@gmx.de>
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
#ifdef HAVE_GLIB
#include <glib.h>       /* g_malloc */
#endif

#include "libcommon.h"
#include "../common.h"

#define PFX "[memdisc] "
#define HOOK_MALLOC 1
#define HOOK_CALLOC 1
#define HOOK_FREE 1
/* GLIB hooks */
#define HOOK_G_MALLOC 1
#define HOOK_G_MALLOC0 1
#define HOOK_G_MALLOC_N 1
#define HOOK_G_MALLOC0_N 1
#define HOOK_G_FREE 1
#define HOOK_G_SLICE_ALLOC 1
#define HOOK_G_SLICE_ALLOC0 1
#define HOOK_G_SLICE_FREE1 1
#define BUF_SIZE PIPE_BUF
#define DYNMEM_IN  "/tmp/memhack_in"
#define DYNMEM_OUT "/tmp/memhack_out"
//#define WRITE_UNCACHED 1
#define FLUSH_INTERVAL 1	/* in seconds */
#define MAX_BT 11		/* for reverse stack search only */

/*
 * Ask gcc for the current stack frame pointer.
 * We don't use the stack pointer as we are not interested in the
 * stuff we have ourselves on the stack and for arch independence.
 */
#ifndef FIRST_FRAME_POINTER
	# define FIRST_FRAME_POINTER  __builtin_frame_address (0)
#endif


/* Hooking control (avoid recursion)
 *
 * For details see:
 * http://www.slideshare.net/tetsu.koba/tips-of-malloc-free
 */
static __thread bool no_hook = false;

/* File descriptors and output buffer */
static i32 ifd = -1;
static FILE *ofile = NULL;  /* much data - we need caching */

/* Output control */
static bool active = false;
static bool discover_ptr = false;
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

/* ask libc for our process name as 'argv[0]' and 'getenv("_")'
   don't work here */
extern char *__progname;

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

/* Config structure for pointer to heap object discovery */
struct cfg {
	size_t mem_size;
	void *code_addr;
	void *stack_offs;
	void *mem_addr;   /* filled by malloc */
	void *ptr_offs;
};
typedef struct cfg cfg_s;

static cfg_s ptr_cfg;


#define READ_STAGE_CFG()  \
	rbytes = read(ifd, ibuf + ioffs, sizeof(ibuf) - ioffs); \
	if (rbytes <= 0) { \
		pr_err("Can't read config for stage %c, " \
			"disabling output.\n", ibuf[0]); \
		return; \
	}

/*
 * flush the output FIFO periodically
 *
 * A fork() doesn't work here. Must be the same
 * process. Hopefully the game doesn't use SIGALRM.
 */
static void flush_output (int signum)
{
	pr_dbg("fflush(ofile) triggered by SIGALRM\n");
	fflush(ofile);
	alarm(FLUSH_INTERVAL);
}

/* prepare memory discovery upon library load */
void __attribute ((constructor)) memdisc_init (void)
{
	char *proc_name = NULL, *expected = NULL;
	ssize_t rbytes;
	i32 read_tries, ioffs = 0;
	char *iptr;
	char ibuf[128] = { 0 };
	char gbt_buf[sizeof(GBT_CMD)] = { 0 };
	void *heap_start = NULL, *heap_soffs = NULL, *heap_eoffs = NULL;

	/* only care for the game process (ignore shell and others) */
	expected = getenv(UGT_GAME_PROC_NAME);
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
	/*
	 * Don't you dare call malloc for another purpose in this lib!
	 * We can only do this safely as (active == false).
	 */
	heap_start = malloc(1);
	if (heap_start) {
		pr_out("Heap start: %p\n", heap_start);
		heap_saddr = heap_eaddr = heap_start;
		free(heap_start);
	}

	ifd = open(DYNMEM_IN, O_RDONLY | O_NONBLOCK);
	if (ifd < 0) {
		perror(PFX "open input");
		exit(1);
	}
	pr_dbg("ifd: %d\n", ifd);

	pr_out("Waiting for output FIFO opener..\n");
	ofile = fopen(DYNMEM_OUT, "w");
	if (!ofile) {
		perror(PFX "fopen output");
		exit(1);
	}
	pr_dbg("ofile: %p\n", ofile);

	for (read_tries = 5; ; --read_tries) {
		rbytes = read(ifd, ibuf, 1);
		if (rbytes == 1)
			break;
		if (read_tries <= 0)
			goto read_err;
		usleep(250 * 1000);
	}
	ioffs = 1;

	fprintf(ofile, "h%p\n", heap_start);
#ifdef WRITE_UNCACHED
	fflush(ofile);
#endif
	memset(&ptr_cfg, 0, sizeof(ptr_cfg));
	if (ibuf[0] == 'p') {
		READ_STAGE_CFG();
		if (sscanf(ibuf + ioffs, ";%zd;%p;%p;%p", &ptr_cfg.mem_size,
		    &ptr_cfg.code_addr, &ptr_cfg.stack_offs,
		    &ptr_cfg.ptr_offs) != 4)
			goto parse_err;
		iptr = strstr(ibuf, ";;");
	        if (!iptr)
			goto parse_err;
		iptr = PTR_SUB(char *, iptr, ibuf);
		ioffs = (ptr_t) iptr + 2;
		discover_ptr = true;

		pr_dbg("ibuf: %s\n", ibuf);
		pr_dbg("ioffs; %d\n", ioffs);
		pr_dbg("ptr_cfg: %zd;%p;%p;%p\n", ptr_cfg.mem_size,
			ptr_cfg.code_addr, ptr_cfg.stack_offs,
			ptr_cfg.ptr_offs);
	} else if (ibuf[0] >= '1' && ibuf[0] <= '5') {
		READ_STAGE_CFG();
		ioffs = 0;
	}

	switch (*(ibuf + ioffs)) {
	/*
	 * stage 1: Find malloc size  (together with static memory search)
	 *	There are lots of mallocs and frees - we need to filter the
	 *	output for a distinct memory area (on the heap) determined
	 *	by static memory search. The interesting bit is a malloc
	 *	without a free where (mem_addr <= found_addr < mem_addr+size).
	 */
	case '1':
		ioffs += 1;
		if (sscanf(ibuf + ioffs, ";%p;%p", &heap_soffs,
		    &heap_eoffs) == 2) {
			heap_saddr = PTR_ADD(void *, heap_saddr, heap_soffs);
			heap_eaddr = PTR_ADD(void *, heap_eaddr, heap_eoffs);
			stage = 1;
			pr_dbg("stage 1 cfg: %p;%p\n", heap_soffs,
				heap_eoffs);
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
		ioffs += 1;
		if (sscanf(ibuf + ioffs, ";%p;%p;%zd", &heap_soffs, &heap_eoffs,
		    &malloc_size) == 3) {
			heap_saddr = PTR_ADD(void *, heap_saddr, heap_soffs);
			heap_eaddr = PTR_ADD(void *, heap_eaddr, heap_eoffs);
			stage = 2;
			pr_dbg("stage 2 cfg: %p;%p;%zd\n", heap_soffs,
				heap_eoffs, malloc_size);
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
		ioffs += 1;
		if (sscanf(ibuf + ioffs, ";%3s;", gbt_buf) == 1 &&
		    strncmp(gbt_buf, GBT_CMD, sizeof(GBT_CMD) - 1) == 0) {
			use_gbt = true;
			ioffs += sizeof(GBT_CMD);
		}
		if (sscanf(ibuf + ioffs, ";%p;%p;%zd;%p;%p", &heap_soffs,
		    &heap_eoffs, &malloc_size, &bt_saddr, &bt_eaddr) == 5) {
			heap_saddr = PTR_ADD(void *, heap_saddr, heap_soffs);
			heap_eaddr = PTR_ADD(void *, heap_eaddr, heap_eoffs);
			if (malloc_size < 1)
				use_gbt = false;
			if (use_gbt)
				pr_out("Using GNU backtrace(). "
					"This might crash with SIGSEGV!\n");
			stage = 3;
			pr_dbg("stage 3 cfg: %p;%p;%zd;%p;%p\n",
				heap_soffs, heap_eoffs, malloc_size,
				bt_saddr, bt_eaddr);
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
		ioffs += 1;
		if (sscanf(ibuf + ioffs, ";%p;%p;%zd;%p;%p;%p", &heap_soffs,
		    &heap_eoffs, &malloc_size, &bt_saddr, &bt_eaddr,
		    &code_addr) >= 5) {
			heap_saddr = PTR_ADD(void *, heap_saddr, heap_soffs);
			heap_eaddr = PTR_ADD(void *, heap_eaddr, heap_eoffs);
			stage = 4;
			pr_dbg("stage 4 cfg: %p;%p;%zd;%p;%p;%p\n",
				heap_soffs, heap_eoffs, malloc_size,
				bt_saddr, bt_eaddr, code_addr);
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

	if (stage > 0 && stage <= 5)
		active = true;

	signal(SIGALRM, flush_output);
	alarm(FLUSH_INTERVAL);

	return;
read_err:
	pr_err("Can't read config, disabling output.\n");
	return;
parse_err:
	pr_err("Error while discovery input parsing! Ignored.\n");
	return;
}

/* clean up upon library unload */
void __attribute ((destructor)) memdisc_exit (void)
{
	if (ofile) {
		fflush(ofile);
		fclose(ofile);
	}
}

/*
 * Get a specific pointer value, pointing to a heap address:
 * 1. from within another heap object
 * 2. from a static memory address
 *
 * Assumption: (discover_ptr == true)
 */
static void get_ptr_to_heap (size_t size, void *mem_addr, void *ffp,
			     char *obuf, i32 *obuf_offs)
{
	void *stack_addr, *ptr_addr = NULL;
	static void *old_ptr_addr = NULL;

	if (ptr_cfg.code_addr) {
		if (size == ptr_cfg.mem_size) {
			stack_addr = PTR_ADD(void *, ffp, ptr_cfg.stack_offs);
			if (stack_addr <= __libc_stack_end - sizeof(void *) &&
			    *(ptr_t *) stack_addr == (ptr_t) ptr_cfg.code_addr)
				ptr_cfg.mem_addr = mem_addr;
		}
		if (ptr_cfg.mem_addr) {
			ptr_addr = PTR_ADD(void *, ptr_cfg.mem_addr,
					   ptr_cfg.ptr_offs);
			ptr_addr = (void *) (*(ptr_t *) ptr_addr);
		}
	} else if (ptr_cfg.ptr_offs) {
		ptr_addr = (void *) (*(ptr_t *) ptr_cfg.ptr_offs);
	}
	if (ptr_addr && ptr_addr != old_ptr_addr) {
		*obuf_offs += sprintf(obuf + *obuf_offs, "p%p\n", ptr_addr);
		old_ptr_addr = ptr_addr;
	}
}

#if DEBUG_MEM && 0
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
static bool find_code_pointers (void *ffp, char *obuf, i32 *obuf_offs)
{
	void *offs, *code_ptr;
	i32 i = 0;
	bool found = false;

	/*
	 * check if we are in the correct section
	 * -> we shouldn't be more that 16 MiB away
	 */
	if (!ffp || ffp < __libc_stack_end - (1 << 24))
		return false;

	for (offs = ffp;
	     offs <= __libc_stack_end - sizeof(void *);
	     offs += sizeof(void *)) {
		code_ptr = (void *) *(ptr_t *) offs;
		if (code_ptr >= bt_saddr && code_ptr <= bt_eaddr) {
			if (stage == 4 &&
			    (code_addr == NULL ||
			     code_ptr == code_addr)) {
				*obuf_offs += sprintf(obuf + *obuf_offs,
					";c%p;o%p", code_ptr,
					(void *) (offs - ffp));
				found = true;
			} else if (stage == 3) {
				*obuf_offs += sprintf(obuf + *obuf_offs,
					";c%p", code_ptr);
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
static bool run_gnu_backtrace (char *obuf, i32 *obuf_offs)
{
	bool found = false;
	void *trace[MAX_GNUBT] = { NULL };
	i32 i, num_taddr = 0;

	num_taddr = backtrace(trace, MAX_GNUBT);
	if (num_taddr > 1) {
		/* skip the first code addr (our own one) */
		for (i = 1; i < num_taddr; i++) {
			if (trace[i] >= bt_saddr && trace[i] <= bt_eaddr) {
				*obuf_offs += sprintf(obuf + *obuf_offs,
					";c%p", trace[i]);
				found = true;
			}
		}
	}
	return found;
}

static inline void postprocess_malloc (void *ffp, size_t size, void *mem_addr)
{
	i32 wbytes;
	char obuf[BUF_SIZE];
	i32 obuf_offs = 0;
	bool found;

	if (active && mem_addr >= heap_saddr && mem_addr < heap_eaddr) {
		if (size == 0 || (malloc_size > 0 && size != malloc_size &&
		    size != ptr_cfg.mem_size))
			goto out;
		obuf_offs = sprintf(obuf, "m%p;s%zd", mem_addr, size);

		if (stage >= 3) {
			dump_stack_raw(ffp);  /* debugging only */
			if (use_gbt)
				found = run_gnu_backtrace(obuf, &obuf_offs);
			else
				found = find_code_pointers(ffp, obuf,
					&obuf_offs);
			if (!found)
				goto out;
		}
		obuf_offs += sprintf(obuf + obuf_offs, "\n");

		if (discover_ptr)
			get_ptr_to_heap(size, mem_addr, ffp, obuf, &obuf_offs);
#if DEBUG_MEM
		pr_out("%s", obuf);
#endif
		wbytes = fprintf(ofile, "%s", obuf);
		if (wbytes < 0) {
			//perror(PFX "fprintf");
			//exit(1);
		}
#ifdef WRITE_UNCACHED
		fflush(ofile);
#endif
	}
out:
	return;
}

static inline void preprocess_free (void *mem_addr)
{
	i32 wbytes;
	char obuf[BUF_SIZE];

	if (active && mem_addr >= heap_saddr && mem_addr < heap_eaddr) {
		sprintf(obuf, "f%p\n", mem_addr);
#if DEBUG_MEM
		pr_out("f%p\n", mem_addr);
#endif
		wbytes = fprintf(ofile, "%s", obuf);
		if (wbytes < 0) {
			//perror(PFX "fprintf");
			//exit(1);
		}
#ifdef WRITE_UNCACHED
		fflush(ofile);
#endif
	}
}

/* void *malloc (size_t size); */
/* void *calloc (size_t nmemb, size_t size); */
/* void *realloc (void *ptr, size_t size); */
/* void free (void *ptr); */

#ifdef HOOK_MALLOC
void *malloc (size_t size)
{
	void *ffp = FIRST_FRAME_POINTER;
	void *mem_addr;
	static void *(*orig_malloc)(size_t size) = NULL;

	if (no_hook)
		return orig_malloc(size);

	/* get the libc malloc function */
	no_hook = true;
	if (!orig_malloc)
		*(void **) (&orig_malloc) = dlsym(RTLD_NEXT, "malloc");

	mem_addr = orig_malloc(size);

	postprocess_malloc(ffp, size, mem_addr);
	no_hook = false;

	return mem_addr;
}
#endif

#ifdef HOOK_CALLOC
/*
 * ATTENTION: The calloc function is special!
 *
 * The first calloc() call must come from static
 * memory as we can't get the libc calloc pointer
 * for it. There will be no free() for it.
 *
 * For details see:
 * http://www.slideshare.net/tetsu.koba/tips-of-malloc-free
 */
static char stat_calloc_space[BUF_SIZE] = { 0 };

/*
 * Static memory allocation for first calloc.
 *
 * Call this only once and don't use functions
 * which might call malloc() here!
 */
static void *stat_calloc (size_t size) {
	static off_t offs = 0;
	void *mem_addr;

	mem_addr = (void *) (stat_calloc_space + offs);
	offs += size;

	if (offs >= sizeof(stat_calloc_space)) {
		offs = sizeof(stat_calloc_space);
		return NULL;
	}
	return mem_addr;
}

void *calloc (size_t nmemb, size_t size)
{
	void *ffp = FIRST_FRAME_POINTER;
	void *mem_addr;
	size_t full_size = nmemb * size;
	static void *(*orig_calloc)(size_t nmemb, size_t size) = NULL;

	if (no_hook) {
		if (!orig_calloc)
			return stat_calloc(full_size);
		return orig_calloc(nmemb, size);
	}

	/* get the libc calloc function */
	no_hook = true;
	if (!orig_calloc)
		*(void **) (&orig_calloc) = dlsym(RTLD_NEXT, "calloc");

	mem_addr = orig_calloc(nmemb, size);

	postprocess_malloc(ffp, full_size, mem_addr);
	no_hook = false;

	return mem_addr;
}
#endif

#ifdef HOOK_FREE
void free (void *ptr)
{
	static void (*orig_free)(void *ptr) = NULL;

	if (no_hook) {
		orig_free(ptr);
		return;
	}

	no_hook = true;
	if (stage == 1)
		preprocess_free(ptr);
	/* get the libc free function */
	if (!orig_free)
		*(void **) (&orig_free) = dlsym(RTLD_NEXT, "free");

	orig_free(ptr);
	no_hook = false;
}
#endif

#ifdef HAVE_GLIB
/* gpointer g_malloc (gsize n_bytes); */
/* gpointer g_malloc0 (gsize n_bytes); */
/* gpointer g_malloc_n (gsize n_blocks, gsize n_block_bytes) */
/* gpointer g_malloc0_n (gsize n_blocks, gsize n_block_bytes) */
/* void g_free (gpointer mem); */
/* gpointer g_slice_alloc (gsize block_size); */
/* gpointer g_slice_alloc0 (gsize block_size); */
/* gpointer g_slice_free1 (gsize block_size, gpointer mem_block); */

#ifdef HOOK_G_MALLOC
gpointer g_malloc (gsize n_bytes)
{
	void *ffp = FIRST_FRAME_POINTER;
	gpointer mem_addr;
	static gpointer (*orig_g_malloc)(gsize n_bytes) = NULL;

	if (no_hook)
		return orig_g_malloc(n_bytes);

	/* get the glib g_malloc function */
	no_hook = true;
	if (!orig_g_malloc)
		*(void **) (&orig_g_malloc) = dlsym(RTLD_NEXT, "g_malloc");

	mem_addr = orig_g_malloc(n_bytes);

	postprocess_malloc(ffp, n_bytes, (void *) mem_addr);
	no_hook = false;

	return mem_addr;
}
#endif

#ifdef HOOK_G_MALLOC0
gpointer g_malloc0 (gsize n_bytes)
{
	void *ffp = FIRST_FRAME_POINTER;
	gpointer mem_addr;
	static gpointer (*orig_g_malloc0)(gsize n_bytes) = NULL;

	if (no_hook)
		return orig_g_malloc0(n_bytes);

	/* get the glib g_malloc0 function */
	no_hook = true;
	if (!orig_g_malloc0)
		*(void **) (&orig_g_malloc0) = dlsym(RTLD_NEXT, "g_malloc0");

	mem_addr = orig_g_malloc0(n_bytes);

	postprocess_malloc(ffp, n_bytes, (void *) mem_addr);
	no_hook = false;

	return mem_addr;
}
#endif

#ifdef HOOK_G_MALLOC_N
gpointer g_malloc_n (gsize n_blocks, gsize n_block_bytes)
{
	void *ffp = FIRST_FRAME_POINTER;
	gpointer mem_addr;
	static gpointer (*orig_g_malloc_n)
		(gsize n_blocks, gsize n_block_bytes) = NULL;

	if (no_hook)
		return orig_g_malloc_n(n_blocks, n_block_bytes);

	/* get the glib g_malloc_n function */
	no_hook = true;
	if (!orig_g_malloc_n)
		*(void **) (&orig_g_malloc_n) = dlsym(RTLD_NEXT, "g_malloc_n");

	mem_addr = orig_g_malloc_n(n_blocks, n_block_bytes);

	postprocess_malloc(ffp, n_blocks * n_block_bytes, (void *) mem_addr);
	no_hook = false;

	return mem_addr;
}
#endif

#ifdef HOOK_G_MALLOC0_N
gpointer g_malloc0_n (gsize n_blocks, gsize n_block_bytes)
{
	void *ffp = FIRST_FRAME_POINTER;
	gpointer mem_addr;
	static gpointer (*orig_g_malloc0_n)
		(gsize n_blocks, gsize n_block_bytes) = NULL;

	if (no_hook)
		return orig_g_malloc0_n(n_blocks, n_block_bytes);

	/* get the glib g_malloc0_n function */
	no_hook = true;
	if (!orig_g_malloc0_n)
		*(void **) (&orig_g_malloc0_n) =
			dlsym(RTLD_NEXT, "g_malloc0_n");

	mem_addr = orig_g_malloc0_n(n_blocks, n_block_bytes);

	postprocess_malloc(ffp, n_blocks * n_block_bytes, (void *) mem_addr);
	no_hook = false;

	return mem_addr;
}
#endif

#ifdef HOOK_G_FREE
void g_free (gpointer mem)
{
	static void (*orig_g_free)(gpointer mem) = NULL;

	if (no_hook) {
		orig_g_free(mem);
		return;
	}

	no_hook = true;
	if (stage == 1)
		preprocess_free((void *) mem);
	/* get the glib g_free function */
	if (!orig_g_free)
		*(void **) (&orig_g_free) = dlsym(RTLD_NEXT, "g_free");

	orig_g_free(mem);
	no_hook = false;
}
#endif

#ifdef HOOK_G_SLICE_ALLOC
gpointer g_slice_alloc (gsize block_size)
{
	void *ffp = FIRST_FRAME_POINTER;
	gpointer mem_addr;
	static gpointer (*orig_g_slice_alloc)(gsize block_size) = NULL;

	if (no_hook)
		return orig_g_slice_alloc(block_size);

	/* get the glib g_slice_alloc function */
	no_hook = true;
	if (!orig_g_slice_alloc)
		*(void **) (&orig_g_slice_alloc) =
			dlsym(RTLD_NEXT, "g_slice_alloc");

	mem_addr = orig_g_slice_alloc(block_size);

	postprocess_malloc(ffp, block_size, (void *) mem_addr);
	no_hook = false;

	return mem_addr;
}
#endif

#ifdef HOOK_G_SLICE_ALLOC0
gpointer g_slice_alloc0 (gsize block_size)
{
	void *ffp = FIRST_FRAME_POINTER;
	gpointer mem_addr;
	static gpointer (*orig_g_slice_alloc0)(gsize block_size) = NULL;

	if (no_hook)
		return orig_g_slice_alloc0(block_size);

	/* get the glib g_slice_alloc0 function */
	no_hook = true;
	if (!orig_g_slice_alloc0)
		*(void **) (&orig_g_slice_alloc0) =
			dlsym(RTLD_NEXT, "g_slice_alloc0");

	mem_addr = orig_g_slice_alloc0(block_size);

	postprocess_malloc(ffp, block_size, (void *) mem_addr);
	no_hook = false;

	return mem_addr;
}
#endif

#ifdef HOOK_G_SLICE_FREE1
void g_slice_free1 (gsize block_size, gpointer mem_block)
{
	static void (*orig_g_slice_free1)
		(gsize block_size, gpointer mem_block) = NULL;

	if (no_hook) {
		orig_g_slice_free1(block_size, mem_block);
		return;
	}

	no_hook = true;
	if (stage == 1)
		preprocess_free((void *) mem_block);
	/* get the glib g_slice_free1 function */
	if (!orig_g_slice_free1)
		*(void **) (&orig_g_slice_free1) =
			dlsym(RTLD_NEXT, "g_slice_free1");

	orig_g_slice_free1(block_size, mem_block);
	no_hook = false;
}
#endif

#endif /* HAVE_GLIB */
