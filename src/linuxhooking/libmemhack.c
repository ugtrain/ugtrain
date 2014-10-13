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

#define ADDR_INVAL 1
#define NUM_CFG_PAGES sizeof(ptr_t) * 1
#define MAX_CFG 5

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
#define stack_end (ptr_t) __libc_stack_end

/* ask libc for our process name as 'argv[0]' and 'getenv("_")'
   don't work here */
extern char *__progname;

/*
 * ATTENTION: GNU backtrace() might crash with SIGSEGV!
 *
 * So use it if explicitly requested only.
 */
static bool use_gbt = false;

/* Config structure */
struct cfg {
	size_t mem_size;
	ptr_t code_addr;
	ptr_t stack_offs;
	u32 max_obj;
	ptr_t *mem_addrs;  /* filled by malloc for free */
};


/*
 * struct cfg pointers followed by cfg structs
 * followed by tracked memory addresses
 * (hacky but aligned to page size)
 */
struct cfg *config[NUM_CFG_PAGES * PIPE_BUF / sizeof(struct cfg *)] = { NULL };


#define SET_IBUF_OFFS(count, i) \
	for (i = 0; i < count; ibuf_offs++) { \
		if (ibuf[ibuf_offs] == ';') \
			i++; \
	}

static inline i32 read_input (char ibuf[], size_t size)
{
	i32 ret = -1;
	i32 read_tries;
	ssize_t rbytes;

	for (read_tries = 5; ; --read_tries) {
		rbytes = read(ifd, ibuf, size);
		if (rbytes > 0) {
			ret = 0;
			break;
		}
		if (read_tries <= 0)
			break;
		usleep(250 * 1000);
	}
	return ret;
}

/* prepare memory hacking upon library load */
void __attribute ((constructor)) memhack_init (void)
{
	char *proc_name = NULL, *expected = NULL;
	char ibuf[BUF_SIZE] = { 0 };
	u32 i, j, k, ibuf_offs = 0, num_cfg = 0, cfg_offs = 0;
	u32 max_obj;
	i32 wbytes, scanned = 0;
	char gbt_buf[sizeof(GBT_CMD)] = { 0 };
	ptr_t code_offs[MAX_CFG] = { 0 };

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
	expected = getenv(UGT_GAME_PROC_NAME);
	if (expected) {
		proc_name = __progname;
		pr_dbg("proc_name: %s, exp: %s\n", proc_name, expected);
		if (strcmp(expected, proc_name) != 0)
			return;
	}

	/* We are preloaded into the right process - stop preloading us!
	   This also hides our presence from the game. ;-) */
	rm_from_env(PRELOAD_VAR, "libmemhack", ':');

	sigignore(SIGPIPE);
	sigignore(SIGCHLD);

	if (active)
		return;

	if (ifd >= 0)
		goto out;
	ifd = open(DYNMEM_IN, O_RDONLY | O_NONBLOCK);
	if (ifd < 0) {
		perror(PFX "open input");
		exit(1);
	}
	pr_dbg("ifd: %d\n", ifd);

	if (ofd >= 0)
		goto out;
	pr_out("Waiting for output FIFO opener..\n");
	ofd = open(DYNMEM_OUT, O_WRONLY | O_TRUNC);
	if (ofd < 0) {
		perror(PFX "open output");
		exit(1);
	}
	pr_dbg("ofd: %d\n", ofd);

	if (read_input(ibuf, sizeof(ibuf)) != 0)
		goto read_err;

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
	cfg_offs = (num_cfg + 1) * sizeof(struct cfg *);  /* NULL for end */
	if (cfg_offs + num_cfg * sizeof(struct cfg) > sizeof(config) ||
	    num_cfg == 0) {
		max_obj = 0;
	} else {
		max_obj = sizeof(config) - cfg_offs - num_cfg *
			sizeof(struct cfg);
		max_obj /= sizeof(ptr_t) * num_cfg;
	}
	if (max_obj <= 1) {
		pr_err("Error: No space for memory addresses!\n");
		goto err;
	} else {
		pr_out("Using max. %u objects per class.\n",
			max_obj - 1);
	}

	pr_dbg("sizeof(config) = %zu\n", sizeof(config));
	pr_dbg("sizeof(struct cfg) = %zu\n", sizeof(struct cfg));

	/* read config into config array */
	for (i = 0; i < num_cfg; i++) {
		config[i] = PTR_ADD2(struct cfg *, config, cfg_offs,
			i * sizeof(struct cfg));

		pr_dbg("&config[%u] pos = %lu\n", i,
			(ulong) ((ptr_t) &config[i] - (ptr_t) config));
		pr_dbg("config[%u] pos = %lu\n", i,
			(ulong) ((ptr_t) config[i] - (ptr_t) config));
		pr_dbg("config[%u] end pos = %lu\n", i,
			(ulong) ((ptr_t) config[i] + sizeof(struct cfg) -
				 (ptr_t) config));

		if ((ptr_t) config[i] + sizeof(struct cfg) - (ptr_t) config
		    > sizeof(config) || ibuf_offs >= BUF_SIZE) {
			/* config doesn't fit */
			pr_err("Config buffer too small!\n");
			goto err;
		}
		scanned = sscanf(ibuf + ibuf_offs, "%zd;" SCN_PTR,
			&config[i]->mem_size, &config[i]->code_addr);
		if (scanned != 2)
			goto err;
		SET_IBUF_OFFS(2, j);
		scanned = sscanf(ibuf + ibuf_offs, SCN_PTR,
				 &config[i]->stack_offs);
		if (scanned != 1)
			goto err;
		SET_IBUF_OFFS(1, j);

		pr_dbg("config: %p, cfg_offs: %u\n", config, cfg_offs);

		/* set the address of the mem addrs array */
		ptr_t addrarr = (ptr_t) config + cfg_offs + num_cfg *
				sizeof(struct cfg) + i * max_obj * sizeof(ptr_t);

		/* debug alignment before dereferencing */
		pr_dbg("setting mem addr array to " PRI_PTR "\n", addrarr);

		/* put stored memory addresses behind all cfg stuctures */
		config[i]->max_obj = max_obj - 1;
		config[i]->mem_addrs = (ptr_t *) addrarr;

		/* debug mem_addrs pointer */
		pr_dbg("config[%u]->mem_addrs pos = %lu\n", i,
			(ulong) ((ptr_t) config[i]->mem_addrs -
				 (ptr_t) config));
		pr_dbg("config[%u]->mem_addrs end pos = %lu\n", i,
			(ulong) ((ptr_t) config[i]->mem_addrs -
				 (ptr_t) config + max_obj * sizeof(ptr_t)));

		if (((ptr_t) config[i]->mem_addrs - (ptr_t) config +
		    (ptr_t) (max_obj * sizeof(ptr_t))) > sizeof(config))
			goto err;

		/* fill with invalid pointers to detect end */
		for (k = 0; k < max_obj; k++)
			config[i]->mem_addrs[k] = ADDR_INVAL;
	}
	if (i != num_cfg)
		goto err;

	/* debug config */
	pr_dbg("num_cfg: %u, cfg_offs: %u\n", num_cfg, cfg_offs);
	for (i = 0; config[i] != NULL; i++) {
		pr_out("config[%u]: mem_size: %zd; "
			"code_addr: " PRI_PTR "; stack_offs: " PRI_PTR "\n", i,
			config[i]->mem_size, config[i]->code_addr,
			config[i]->stack_offs);
	}

	wbytes = write(ofd, "ready\n", sizeof("ready\n"));
	if (wbytes < 0)
		perror("write");
	if (read_input(ibuf, sizeof(ibuf)) != 0) {
		pr_err("Couldn't read code offset!\n");
	} else {
		if (sscanf(ibuf, SCN_PTR ";" SCN_PTR ";" SCN_PTR ";" SCN_PTR
		    ";" SCN_PTR, &code_offs[0], &code_offs[1], &code_offs[2],
		    &code_offs[3], &code_offs[4]) < 1)
			pr_err("Code offset parsing error!\n");
	}
	if (num_cfg <= MAX_CFG) {
		for (i = 0; i < num_cfg; i++)
			config[i]->code_addr += code_offs[i];
	}
	if (num_cfg > 0)
		active = true;
out:
	return;

read_err:
	pr_err("Can't read config, disabling output.\n");
	return;
err:
	pr_err("Error while reading config!\n");
	return;
}

/* clean up upon library unload */
void __attribute ((destructor)) memhack_exit (void)
{
#if USE_DEBUG_LOG
	if (DBG_FILE_VAR) {
		fflush(DBG_FILE_VAR);
		fclose(DBG_FILE_VAR);
	}
#endif
}

static inline void postprocess_malloc (ptr_t ffp, size_t size, ptr_t mem_addr)
{
	ptr_t stack_addr;
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
					if ((ptr_t) trace[j] == config[i]->code_addr)
						goto found;
				}
				continue;
			}

			/* reverse stack offset method */
			stack_addr = ffp + config[i]->stack_offs;

			if (stack_addr > stack_end - sizeof(ptr_t) ||
			    *(ptr_t *) stack_addr != config[i]->code_addr)
				continue;
found:
#if DEBUG_MEM
			pr_out("malloc: mem_addr: " PRI_PTR ", code_addr: "
				PRI_PTR "\n", mem_addr, config[i]->code_addr);
#endif
			wbytes = sprintf(obuf, "m" PRI_PTR ";c" PRI_PTR "\n",
				mem_addr, config[i]->code_addr);

			wbytes = write(ofd, obuf, wbytes);
			if (wbytes < 0) {
				perror("write");
			} else if (config[i]->mem_addrs) {
				for (j = 0; j < config[i]->max_obj; j++) {
					if (config[i]->mem_addrs[j] <= ADDR_INVAL) {
						config[i]->mem_addrs[j] = mem_addr;
						break;
					}
				}
			}
			break;
		}
	}
}

static inline void preprocess_free (ptr_t mem_addr)
{
	i32 i, j, wbytes;

	if (active && mem_addr) {
		for (i = 0; config[i] != NULL; i++) {
			if (!config[i]->mem_addrs)
				continue;
			for (j = 0; config[i]->mem_addrs[j] != ADDR_INVAL; j++) {
				if (config[i]->mem_addrs[j] == mem_addr)
					goto found;
			}
			continue;
found:
#if DEBUG_MEM
			pr_out("free: mem_addr: " PRI_PTR "\n", mem_addr);
#endif
			wbytes = sprintf(obuf, "f" PRI_PTR "\n", mem_addr);

			wbytes = write(ofd, obuf, wbytes);
			if (wbytes < 0)
				perror("write");
			config[i]->mem_addrs[j] = 0;
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
	ptr_t ffp = (ptr_t) FIRST_FRAME_POINTER;
	void *mem_addr;
	static void *(*orig_malloc)(size_t size) = NULL;

	if (no_hook)
		return orig_malloc(size);

	/* get the libc malloc function */
	no_hook = true;
	if (!orig_malloc)
		*(void **) (&orig_malloc) = dlsym(RTLD_NEXT, "malloc");

	mem_addr = orig_malloc(size);

	postprocess_malloc(ffp, size, (ptr_t) mem_addr);
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
	ptr_t ffp = (ptr_t) FIRST_FRAME_POINTER;
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

	postprocess_malloc(ffp, full_size, (ptr_t) mem_addr);
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
	preprocess_free((ptr_t) ptr);
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
	ptr_t ffp = (ptr_t) FIRST_FRAME_POINTER;
	gpointer mem_addr;
	static gpointer (*orig_g_malloc)(gsize n_bytes) = NULL;

	if (no_hook)
		return orig_g_malloc(n_bytes);

	/* get the glib g_malloc function */
	no_hook = true;
	if (!orig_g_malloc)
		*(void **) (&orig_g_malloc) = dlsym(RTLD_NEXT, "g_malloc");

	mem_addr = orig_g_malloc(n_bytes);

	postprocess_malloc(ffp, n_bytes, (ptr_t) mem_addr);
	no_hook = false;

	return mem_addr;
}
#endif

#ifdef HOOK_G_MALLOC0
gpointer g_malloc0 (gsize n_bytes)
{
	ptr_t ffp = (ptr_t) FIRST_FRAME_POINTER;
	gpointer mem_addr;
	static gpointer (*orig_g_malloc0)(gsize n_bytes) = NULL;

	if (no_hook)
		return orig_g_malloc0(n_bytes);

	/* get the glib g_malloc0 function */
	no_hook = true;
	if (!orig_g_malloc0)
		*(void **) (&orig_g_malloc0) = dlsym(RTLD_NEXT, "g_malloc0");

	mem_addr = orig_g_malloc0(n_bytes);

	postprocess_malloc(ffp, n_bytes, (ptr_t) mem_addr);
	no_hook = false;

	return mem_addr;
}
#endif

#ifdef HOOK_G_MALLOC_N
gpointer g_malloc_n (gsize n_blocks, gsize n_block_bytes)
{
	ptr_t ffp = (ptr_t) FIRST_FRAME_POINTER;
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

	postprocess_malloc(ffp, n_blocks * n_block_bytes, (ptr_t) mem_addr);
	no_hook = false;

	return mem_addr;
}
#endif

#ifdef HOOK_G_MALLOC0_N
gpointer g_malloc0_n (gsize n_blocks, gsize n_block_bytes)
{
	ptr_t ffp = (ptr_t) FIRST_FRAME_POINTER;
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

	postprocess_malloc(ffp, n_blocks * n_block_bytes, (ptr_t) mem_addr);
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
	preprocess_free((ptr_t) mem);
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
	ptr_t ffp = (ptr_t) FIRST_FRAME_POINTER;
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

	postprocess_malloc(ffp, block_size, (ptr_t) mem_addr);
	no_hook = false;

	return mem_addr;
}
#endif

#ifdef HOOK_G_SLICE_ALLOC0
gpointer g_slice_alloc0 (gsize block_size)
{
	ptr_t ffp = (ptr_t) FIRST_FRAME_POINTER;
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

	postprocess_malloc(ffp, block_size, (ptr_t) mem_addr);
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
	preprocess_free((ptr_t) mem_block);
	/* get the glib g_slice_free1 function */
	if (!orig_g_slice_free1)
		*(void **) (&orig_g_slice_free1) =
			dlsym(RTLD_NEXT, "g_slice_free1");

	orig_g_slice_free1(block_size, mem_block);
	no_hook = false;
}
#endif

#endif /* HAVE_GLIB */
