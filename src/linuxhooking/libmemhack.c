/* libmemhack.c:    hacking of unique malloc calls (used by ugtrain)
 *
 * Copyright (c) 2012..2015 Sebastian Parschauer <s.parschauer@gmx.de>
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
#include <pthread.h>    /* pthread_mutex_lock */
#ifdef HAVE_GLIB
#include <glib.h>       /* g_malloc */
#endif

#include "libcommon.h"

#define PFX "[memhack] "
#define HOOK_MALLOC 1
#define HOOK_CALLOC 1
#define HOOK_REALLOC 1
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

/* Grow structure */
struct grow {
	size_t size_min;
	size_t size_max;
	enum grow_type type;
	u32 add;
	ptr_t code_addr;
	ptr_t stack_offs;
};

/* Config structure */
struct cfg {
	size_t mem_size;
	ptr_t code_addr;
	ptr_t stack_offs;
	ptr_t *mem_addrs;  /* filled by malloc for free */
	u32 max_obj;       /* currently allocated memory addresses */
	u32 insert_idx;    /* lowest mem_addrs index for insertion */
	pthread_mutex_t mutex;  /* protects mem_addrs, max_obj, insert_idx */
	struct grow *grow;
};

/*
 * struct cfg pointers array
 */
static struct cfg **config = NULL;


#define SET_IBUF_OFFS() {				\
	char *pos = strchr(&ibuf[ibuf_offs], ';');	\
	if (pos)					\
		ibuf_offs = pos - ibuf + 1;		\
}

static inline i32 read_input (char ibuf[], size_t size)
{
	i32 ret = -1;
	i32 read_tries;
	ssize_t rbytes;

	for (read_tries = 5; ; --read_tries) {
		rbytes = read(ifd, ibuf, size);
		if (rbytes > 0) {
			ibuf[rbytes] = 0;
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
	u32 i, j, k, ibuf_offs = 0, num_cfg = 0;
	i32 wbytes, scanned = 0;
	char gbt_buf[sizeof(GBT_CMD)] = { 0 };
	ptr_t code_offs[MAX_CFG] = { 0 };
	struct cfg *cfg_sa = NULL;  /* struct cfg array */
	struct grow *grow = NULL;

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

	if (read_input(ibuf, sizeof(ibuf) - 1) != 0)
		goto read_err;

	scanned = sscanf(ibuf + ibuf_offs, "%u;", &num_cfg);
	if (scanned != 1 || num_cfg <= 0)
		goto err;
	SET_IBUF_OFFS();
	if (sscanf(ibuf + ibuf_offs, "%3s;", gbt_buf) == 1 &&
	    strncmp(gbt_buf, GBT_CMD, sizeof(GBT_CMD) - 1) == 0) {
		use_gbt = true;
		SET_IBUF_OFFS();
		pr_out("Using GNU backtrace(). "
			"This might crash with SIGSEGV!\n");
	}
	if (num_cfg > MAX_CFG) {
		pr_err("Error: Too many classes configured!\n");
		num_cfg = MAX_CFG;
	}
	config = (struct cfg **) calloc(num_cfg + 1, sizeof(struct cfg *));
		/* NULL for end */
	if (!config) {
		pr_err("Error: No space for struct cfg addresses!\n");
		goto err;
	}
	cfg_sa = (struct cfg *) calloc(num_cfg, sizeof(struct cfg));
	if (!cfg_sa) {
		pr_err("Error: No space for cfg structures!\n");
		goto err;
	}

	pr_dbg("sizeof(config) = %zu\n", (num_cfg + 1) * sizeof(struct cfg *));
	pr_dbg("sizeof(struct cfg) = %zu\n", sizeof(struct cfg));
	pr_dbg("sizeof(pthread_mutex_t) = %zu\n", sizeof(pthread_mutex_t));

	/* read config into config array */
	for (i = 0; i < num_cfg; i++) {
		char op_ch = -1;
		config[i] = PTR_ADD(struct cfg *, cfg_sa,
			i * sizeof(struct cfg));

		scanned = sscanf(ibuf + ibuf_offs, "%zd;" SCN_PTR ";" SCN_PTR,
			&config[i]->mem_size, &config[i]->code_addr,
			&config[i]->stack_offs);
		if (scanned != 3)
			goto err;
		for (j = 3; j > 0; --j)
			SET_IBUF_OFFS();

		pr_dbg("config: %p\n", config);

		/* allocate the mem addrs array to store one object */
		config[i]->mem_addrs = (ptr_t *) calloc(2, sizeof(ptr_t));
		if (!config[i]->mem_addrs) {
			pr_err("Error: No space for memory addresses!\n");
			goto err;
		}
		config[i]->max_obj = 1;

		/* fill with invalid pointers to detect end */
		for (k = 0; k <= config[i]->max_obj; k++)
			config[i]->mem_addrs[k] = ADDR_INVAL;

		pthread_mutex_init(&config[i]->mutex, NULL);

		/* handle growing config */
		if (strncmp(ibuf + ibuf_offs, "grow;", 5) != 0)
			continue;
		SET_IBUF_OFFS();
		grow = (struct grow *) calloc(1, sizeof(struct grow));
		if (!grow) {
			pr_err("Error: No space for grow structure, "
				"ignoring it!\n");
			continue;
		}
		scanned = sscanf(ibuf + ibuf_offs, "%zd;%zd;%c%u;" SCN_PTR ";"
			SCN_PTR, &grow->size_min, &grow->size_max, &op_ch,
			&grow->add, &grow->code_addr, &grow->stack_offs);
		if (scanned != 6 || op_ch != '+') {
			pr_err("Error: Grow parsing failed!\n");
			free(grow);
			goto err;
		}
		grow->type = GROW_ADD;
		for (j = 5; j > 0; --j)
			SET_IBUF_OFFS();
		config[i]->grow = grow;
	}
	if (i != num_cfg)
		goto err;

	/* debug config */
	pr_dbg("num_cfg: %u\n", num_cfg);
	for (i = 0; config[i] != NULL; i++) {
		struct grow *grow = config[i]->grow;
		pr_out("config[%u]: mem_size: %zd; "
			"code_addr: " PRI_PTR "; stack_offs: " PRI_PTR "\n", i,
			config[i]->mem_size, config[i]->code_addr,
			config[i]->stack_offs);
		if (grow) {
			pr_out("config[%u] growing: size_min: %zd; size_max: "
				"%zd; add: %u; code_addr: " PRI_PTR
				"; stack_offs: " PRI_PTR "\n", i,
				grow->size_min, grow->size_max, grow->add,
				grow->code_addr, grow->stack_offs);
		}

	}

	wbytes = write(ofd, "ready\n", sizeof("ready\n"));
	if (wbytes < 0)
		perror("write");
	if (read_input(ibuf, sizeof(ibuf) - 1) != 0) {
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
	u32 i;
#if USE_DEBUG_LOG
	if (DBG_FILE_VAR) {
		fflush(DBG_FILE_VAR);
		fclose(DBG_FILE_VAR);
	}
#endif
	if (!active || !config)
		return;
	for (i = 0; config[i] != NULL; i++)
		pthread_mutex_destroy(&config[i]->mutex);
}

static inline void store_mem_addr (struct cfg *cfg, ptr_t mem_addr)
{
	u32 j;

	if (pthread_mutex_lock(&cfg->mutex) < 0) {
		perror("pthread_mutex_lock");
		goto out;
	}
	if (!cfg->mem_addrs) {
		cfg->max_obj = 0;
		goto out_unlock;
	}
	/* size doubling allocator */
	if (cfg->mem_addrs[cfg->max_obj - 1] != ADDR_INVAL) {
		cfg->mem_addrs = realloc(cfg->mem_addrs,
			((cfg->max_obj << 1) + 1) * sizeof(ptr_t));
		if (!cfg->mem_addrs) {
			cfg->max_obj = 0;
			goto out_unlock;
		}
		for (j = cfg->max_obj + 1; j <= cfg->max_obj << 1; j++)
			cfg->mem_addrs[j] = ADDR_INVAL;
		cfg->max_obj <<= 1;
	}
	/* actual storing */
	for (j = cfg->insert_idx; j < cfg->max_obj; j++) {
		if (cfg->mem_addrs[j] <= ADDR_INVAL) {
			cfg->mem_addrs[j] = mem_addr;
			cfg->insert_idx = j;
			break;
		}
	}
out_unlock:
	if (pthread_mutex_unlock(&cfg->mutex) < 0)
		perror("pthread_mutex_unlock");
out:
	return;
}

static inline void postprocess_malloc (ptr_t ffp, size_t size, ptr_t mem_addr)
{
	ptr_t code_addr, stack_offs, stack_addr;
	i32 i, j, wbytes, num_taddr = 0;
	void *trace[MAX_GNUBT] = { NULL };

	if (!active)
		return;

	for (i = 0; config[i] != NULL; i++) {
		struct grow *grow = config[i]->grow;
		if (size == config[i]->mem_size) {
			code_addr = config[i]->code_addr;
			stack_offs = config[i]->stack_offs;
		} else if (grow && size <= grow->size_max &&
		    size >= grow->size_min) {
			if (size % grow->add != 0)
				continue;
			code_addr = grow->code_addr;
			stack_offs = grow->stack_offs;
		} else {
			continue;
		}
		/* rarely used GNU backtrace method (unstable, slow) */
		if (use_gbt) {
			num_taddr = backtrace(trace, MAX_GNUBT);
			if (num_taddr < 2)
				continue;
			/* skip the first code addr (our own one) */
			for (j = 1; j < num_taddr; j++) {
				if ((ptr_t) trace[j] == code_addr)
					goto found;
			}
			continue;
		}

		/* reverse stack offset method */
		stack_addr = ffp + stack_offs;
		if (stack_addr > stack_end - sizeof(ptr_t))
			continue;
		if (*(ptr_t *) stack_addr == code_addr)
			goto found;
		continue;
found:
#if DEBUG_MEM
		pr_out("malloc: mem_addr: " PRI_PTR ", size: %zu, code_addr: "
			PRI_PTR "\n", mem_addr, size, code_addr);
#endif
		wbytes = sprintf(obuf, "m" PRI_PTR ";s%zu;c" PRI_PTR "\n",
			mem_addr, size, code_addr);

		wbytes = write(ofd, obuf, wbytes);
		if (wbytes < 0)
			perror("write");
		else
			store_mem_addr(config[i], mem_addr);
		break;
	}
}

static inline void preprocess_free (ptr_t mem_addr)
{
	i32 wbytes;
	u32 i, j;

	if (!active || !mem_addr)
		return;

	for (i = 0; config[i] != NULL; i++) {
		if (pthread_mutex_lock(&config[i]->mutex) < 0) {
			perror("pthread_mutex_lock");
			goto cont;
		}
		if (!config[i]->mem_addrs)
			goto unlock_cont;
		for (j = 0; config[i]->mem_addrs[j] != ADDR_INVAL; j++) {
			if (config[i]->mem_addrs[j] == mem_addr)
				goto found;
		}
unlock_cont:
		if (pthread_mutex_unlock(&config[i]->mutex) < 0)
			perror("pthread_mutex_unlock");
cont:
		continue;
found:
		config[i]->mem_addrs[j] = 0;
		if (j < config[i]->insert_idx)
			config[i]->insert_idx = j;
		if (pthread_mutex_unlock(&config[i]->mutex) < 0)
			perror("pthread_mutex_unlock");
#if DEBUG_MEM
		pr_out("free: mem_addr: " PRI_PTR "\n", mem_addr);
#endif
		wbytes = sprintf(obuf, "f" PRI_PTR "\n", mem_addr);

		wbytes = write(ofd, obuf, wbytes);
		if (wbytes < 0)
			perror("write");
		break;
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

#ifdef HOOK_REALLOC
void *realloc (void *ptr, size_t size)
{
	ptr_t ffp = (ptr_t) FIRST_FRAME_POINTER;
	void *mem_addr;
	static void *(*orig_realloc)(void *ptr, size_t size) = NULL;

	if (no_hook)
		return orig_realloc(ptr, size);

	no_hook = true;
	preprocess_free((ptr_t) ptr);
	/* get the libc realloc function */
	if (!orig_realloc)
		*(void **) (&orig_realloc) = dlsym(RTLD_NEXT, "realloc");

	mem_addr = orig_realloc(ptr, size);

	postprocess_malloc(ffp, size, (ptr_t) mem_addr);
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
