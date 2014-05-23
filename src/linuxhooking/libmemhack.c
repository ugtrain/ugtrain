/* libmemhack.c:    hacking of unique malloc calls (used by ugtrain)
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

#define PFX "[memhack] "
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

#define PTR_INVAL (void *) 1
#define NUM_CFG_PAGES sizeof(long) * 1

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
static i32 ofd = -1, ifd = -1;
static char obuf[BUF_SIZE];

/* Output control */
static bool active = false;

/* Stack check */
/* This is a global variable set at program start time. It marks the
   greatest used stack address. */
extern void *__libc_stack_end;

/* ask libc for our process name as 'argv[0]' and 'getenv("_")'
   don't work here */
extern char *__progname;

/* For PIE (position independent executable) we have to determine
   the code start in memory and use it as an offset. */
static void *code_offs = NULL;

/*
 * ATTENTION: GNU backtrace() might crash with SIGSEGV!
 *
 * So use it if explicitly requested only.
 */
static bool use_gbt = false;

/* Config structure */
struct cfg {
	size_t mem_size;
	void *code_addr;
	void *stack_offs;
	u32 max_obj;
	void **mem_addrs;  /* filled by malloc for free */
};
typedef struct cfg cfg_s;


/*
 * cfg_s pointers followed by cfg structs
 * followed by tracked memory addresses
 * (hacky but aligned to page size)
 */
cfg_s *config[NUM_CFG_PAGES * PIPE_BUF / sizeof(cfg_s *)] = { NULL };


#define SET_IBUF_OFFS(count, i) \
	for (i = 0; i < count; ibuf_offs++) { \
		if (ibuf[ibuf_offs] == ';') \
			i++; \
	}

/* prepare memory hacking upon library load */
void __attribute ((constructor)) memhack_init (void)
{
	char *proc_name = NULL, *expected = NULL, *game_binpath = NULL;
	ssize_t rbytes;
	char ibuf[BUF_SIZE] = { 0 };
	u32 i, j, k, ibuf_offs = 0, num_cfg = 0, cfg_offs = 0;
	u32 max_obj;
	i32 read_tries, scanned = 0;
	char gbt_buf[sizeof(GBT_CMD)] = { 0 };

	/* only care for the game process (ignore shell and others) */
	expected = getenv(UGT_GAME_PROC_NAME);
	if (expected) {
		proc_name = __progname;
		pr_dbg("proc_name: %s, exp: %s\n", proc_name, expected);
		if (strcmp(expected, proc_name) != 0)
			return;
	}
	game_binpath = getenv(UGT_GAME_BINPATH);

	/* We are preloaded into the right process - stop preloading us!
	   This also hides our presence from the game. ;-) */
	rm_from_env(PRELOAD_VAR, "libmemhack", ':');

	sigignore(SIGPIPE);
	sigignore(SIGCHLD);

	if (active)
		return;

	ifd = open(DYNMEM_IN, O_RDONLY | O_NONBLOCK);
	if (ifd < 0) {
		perror(PFX "open input");
		exit(1);
	}
	pr_dbg("ifd: %d\n", ifd);

	pr_out("Waiting for output FIFO opener..\n");
	ofd = open(DYNMEM_OUT, O_WRONLY | O_TRUNC);
	if (ofd < 0) {
		perror(PFX "open output");
		exit(1);
	}
	pr_dbg("ofd: %d\n", ofd);

	for (read_tries = 5; ; --read_tries) {
		rbytes = read(ifd, ibuf, sizeof(ibuf));
		if (rbytes > 0)
			break;
		if (read_tries <= 0)
			goto read_err;
		usleep(250 * 1000);
	}

	scanned = sscanf(ibuf + ibuf_offs, "%u", &num_cfg);
	if (scanned != 1)
		goto err;
	SET_IBUF_OFFS(1, j);
	if (sscanf(ibuf + ibuf_offs, "%3s;", gbt_buf) == 1 &&
	    strncmp(gbt_buf, GBT_CMD, sizeof(GBT_CMD) - 1) == 0) {
		use_gbt = true;
		SET_IBUF_OFFS(1, j);
		pr_out("Using GNU backtrace(). "
			"This might crash with SIGSEGV!\n");
	}
	cfg_offs = (num_cfg + 1) * sizeof(cfg_s *);  /* NULL for cfg_s* end */
	if (cfg_offs + num_cfg * sizeof(cfg_s) > sizeof(config) ||
	    num_cfg == 0) {
		max_obj = 0;
	} else {
		max_obj = sizeof(config) - cfg_offs - num_cfg * sizeof(cfg_s);
		max_obj /= sizeof(void *) * num_cfg;
	}
	if (max_obj <= 1) {
		pr_err("Error: No space for memory addresses!\n");
		goto err;
	} else {
		pr_out("Using max. %u objects per class.\n",
			max_obj - 1);
	}

	pr_dbg("sizeof(config) = %lu\n", (ulong) sizeof(config));
	pr_dbg("sizeof(cfg_s) = %lu\n", (ulong) sizeof(cfg_s));

	/* read config into config array */
	for (i = 0; i < num_cfg; i++) {
		config[i] = PTR_ADD2(cfg_s *, config, cfg_offs,
			i * sizeof(cfg_s));

		pr_dbg("&config[%u] pos = %lu\n", i,
			(ulong) PTR_SUB(void *, &config[i], config));
		pr_dbg("config[%u] pos = %lu\n", i,
			(ulong) PTR_SUB(void *, config[i], config));
		pr_dbg("config[%u] end pos = %lu\n", i,
			(ulong)((ptr_t) config[i] + sizeof(cfg_s) -
				(ptr_t) config));

		if ((ptr_t) config[i] + sizeof(cfg_s) - (ptr_t) config
		    > sizeof(config) || ibuf_offs >= BUF_SIZE) {
			/* config doesn't fit */
			pr_err("Config buffer too small!\n");
			goto err;
		}
		scanned = sscanf(ibuf + ibuf_offs, "%zd;%p",
			&config[i]->mem_size, &config[i]->code_addr);
		if (scanned != 2)
			goto err;
		SET_IBUF_OFFS(2, j);
		scanned = sscanf(ibuf + ibuf_offs, "%p",
				 &config[i]->stack_offs);
		if (scanned != 1)
			goto err;
		SET_IBUF_OFFS(1, j);

		/* put stored memory addresses behind all cfg_s stuctures */
		config[i]->max_obj = max_obj - 1;
		config[i]->mem_addrs = PTR_ADD2(void **, config, cfg_offs,
			num_cfg * sizeof(cfg_s));
		config[i]->mem_addrs = PTR_ADD(void **, config[i]->mem_addrs,
			i * max_obj * sizeof(void *));

		/* debug mem_addrs pointer */
		pr_dbg("config[%u]->mem_addrs pos = %lu\n", i,
			(ulong) PTR_SUB(void *, config[i]->mem_addrs, config));
		pr_dbg("config[%u]->mem_addrs end pos = %lu\n", i,
			(ulong) PTR_SUB(void *, config[i]->mem_addrs, config) +
			(ulong) (max_obj * sizeof(void *)));

		if ((ulong) PTR_SUB(void *, config[i]->mem_addrs, config) +
		    (ulong) (max_obj * sizeof(void *)) > sizeof(config))
			goto err;

		/* fill with invalid pointers to detect end */
		for (k = 0; k < max_obj; k++)
			config[i]->mem_addrs[k] = PTR_INVAL;
	}
	if (i != num_cfg)
		goto err;

	/* debug config */
	pr_dbg("num_cfg: %u, cfg_offs: %u\n", num_cfg, cfg_offs);
	for (i = 0; config[i] != NULL; i++) {
		pr_out("config[%u]: mem_size: %zd; "
			"code_addr: %p; stack_offs: %p\n", i,
			config[i]->mem_size, config[i]->code_addr,
			config[i]->stack_offs);
	}

	/* PIE: handle code address offset */
	if (game_binpath)
		code_offs = get_code_offs(-1, game_binpath);
	if (code_offs) {
		pr_dbg("PIE (position independent executable) "
			"detected!\n");
		for (i = 0; i < num_cfg; i++)
			config[i]->code_addr = PTR_ADD(void *,
				code_offs, config[i]->code_addr);
	}
	pr_out("Code offset: %p\n", code_offs);

	if (num_cfg > 0)
		active = true;
	return;

read_err:
	pr_err("Can't read config, disabling output.\n");
	return;
err:
	pr_err("Error while reading config!\n");
	return;
}

static inline void postprocess_malloc (void *ffp, size_t size, void *mem_addr)
{
	void *stack_addr;
	i32 i, j, wbytes, num_taddr = 0;
	void *trace[MAX_GNUBT] = { NULL };

	if (active) {
		for (i = 0; config[i] != NULL; i++) {
			if (size != config[i]->mem_size)
				continue;

			if (use_gbt) {
				num_taddr = backtrace(trace, MAX_GNUBT);
				if (num_taddr < 2)
					continue;
				/* skip the first code addr (our own one) */
				for (j = 1; j < num_taddr; j++) {
					if (trace[j] == config[i]->code_addr)
						goto found;
				}
				continue;
			}

			/* reverse stack offset method */
			stack_addr = PTR_ADD(void *, ffp,
				config[i]->stack_offs);

			if (stack_addr > __libc_stack_end - sizeof(void *) ||
			    *(ptr_t *) stack_addr !=
			    (ptr_t) config[i]->code_addr)
				continue;
found:
			pr_dbg("malloc: mem_addr: %p, code_addr: %p\n",
				mem_addr, PTR_SUB(void *, config[i]->code_addr,
				code_offs));
			wbytes = sprintf(obuf, "m%p;c%p\n",
				mem_addr, PTR_SUB(void *, config[i]->code_addr,
				code_offs));

			wbytes = write(ofd, obuf, wbytes);
			if (wbytes < 0) {
				perror("write");
			} else if (config[i]->mem_addrs) {
				for (j = 0; j < config[i]->max_obj; j++) {
					if (config[i]->mem_addrs[j] <= PTR_INVAL) {
						config[i]->mem_addrs[j] = mem_addr;
						break;
					}
				}
			}
			break;
		}
	}
}

static inline void preprocess_free (void *mem_addr)
{
	i32 i, j, wbytes;

	if (active && mem_addr != NULL) {
		for (i = 0; config[i] != NULL; i++) {
			if (!config[i]->mem_addrs)
				continue;
			for (j = 0; config[i]->mem_addrs[j] != PTR_INVAL; j++) {
				if (config[i]->mem_addrs[j] == mem_addr)
					goto found;
			}
			continue;
found:
			pr_dbg("free: mem_addr: %p\n", mem_addr);
			wbytes = sprintf(obuf, "f%p\n", mem_addr);

			wbytes = write(ofd, obuf, wbytes);
			if (wbytes < 0)
				perror("write");
			config[i]->mem_addrs[j] = NULL;
			break;
		}
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
