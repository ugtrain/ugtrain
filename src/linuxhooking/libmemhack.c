/* libmemhack.c:    hacking of unique malloc calls (used by ugtrain)
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

#define _GNU_SOURCE
#include <stdio.h>      /* printf */
#include <stdlib.h>     /* malloc */
#include <string.h>
#include <fcntl.h>
#include <signal.h>     /* sigignore */
#include <libgen.h>     /* basename */
#include <unistd.h>     /* read */
#include <limits.h>     /* PIPE_BUF */
#include <execinfo.h>   /* backtrace */
#include <pthread.h>    /* pthread_rwlock_init */
#ifdef HAVE_GLIB
#include <glib.h>       /* g_malloc */
#endif

#include "libcommon.h"

#define PFX "[memhack] "
#define DYNMEM_IN  "/tmp/memhack_in"
#define DYNMEM_OUT "/tmp/memhack_out"

#define ADDR_INVAL 1


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
	ptr_t code_offs;
	ptr_t stack_offs;
	char *lib;
};

/* Config structure */
struct cfg {
	size_t mem_size;
	ptr_t code_addr;
	ptr_t code_offs;
	ptr_t stack_offs;
	char *lib;
	struct grow *grow;

	/* later determined */
	ptr_t *mem_addrs;       /* filled by malloc for free */
	u32 max_obj;            /* currently allocated memory addresses */
	u32 insert_idx;         /* lowest mem_addrs index for insertion */
	pthread_rwlock_t lock;  /* protects mem_addrs, max_obj, insert_idx */
};

/*
 * struct cfg pointers array
 */
static struct cfg **config = NULL;


static inline void output_cfg (void)
{
	i32 i;

	for (i = 0; config[i] != NULL; i++) {
		struct grow *grow = config[i]->grow;
		pr_out("config[%u]: mem_size: %zd; "
			"code_addr: " PRI_PTR "; stack_offs: " PRI_PTR "; %s\n", i,
			config[i]->mem_size, config[i]->code_addr,
			config[i]->stack_offs, (config[i]->lib) ?
			config[i]->lib : "exe");
		if (grow) {
			pr_out("config[%u] growing: size_min: %zd; size_max: "
				"%zd; add: %u; code_addr: " PRI_PTR
				"; stack_offs: " PRI_PTR "; %s\n", i,
				grow->size_min, grow->size_max, grow->add,
				grow->code_addr, grow->stack_offs, (grow->lib) ?
				grow->lib : "exe");
		}

	}
}

#define SET_IBUF_START(start) {				\
	char *pos = strchr(start, ';');			\
	if (pos)					\
		start = pos + 1;			\
}

#define SET_CODE_LIB(start, lib) {			\
	char *pos = strchr(start, ';');			\
	size_t size;					\
	if (pos)					\
		*pos = '\0';				\
	if (strcmp(start, "exe") != 0) {		\
		size = strlen(start) + 1;		\
		lib = (char *) malloc(size);		\
		if (lib) {				\
			memcpy(lib, start, size - 1);	\
			lib[size - 1] = '\0';		\
		}					\
	}						\
	if (pos)					\
		start = pos + 1;			\
}

#define RM_NEW_LINE(start) {				\
	char *pos = strchr(start, '\n');		\
	if (pos)					\
		*pos = '\0';				\
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
	char ibuf[BUF_SIZE] = { 0 }, *start = ibuf;
	u32 i, j, k, num_cfg = 0;
	i32 wbytes, scanned = 0;
	char gbt_buf[sizeof(GBT_CMD)] = { 0 };
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

	scanned = sscanf(start, "%u;", &num_cfg);
	if (scanned != 1 || num_cfg <= 0)
		goto err;
	RM_NEW_LINE(start);
	SET_IBUF_START(start);
	if (sscanf(start, "%3s;", gbt_buf) == 1 &&
	    strncmp(gbt_buf, GBT_CMD, sizeof(GBT_CMD) - 1) == 0) {
		use_gbt = true;
		SET_IBUF_START(start);
		pr_out("Using GNU backtrace(). "
			"This might crash with SIGSEGV!\n");
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
	pr_dbg("sizeof(pthread_rwlock_t) = %zu\n", sizeof(pthread_rwlock_t));

	/* read config into config array */
	for (i = 0; i < num_cfg; i++) {
		char op_ch = -1;
		config[i] = PTR_ADD(struct cfg *, cfg_sa,
			i * sizeof(struct cfg));

		scanned = sscanf(start, "%zd;" SCN_PTR ";" SCN_PTR,
			&config[i]->mem_size, &config[i]->code_addr,
			&config[i]->stack_offs);
		if (scanned != 3)
			goto err;
		config[i]->code_offs = config[i]->code_addr;
		for (j = 3; j > 0; --j)
			SET_IBUF_START(start);
		SET_CODE_LIB(start, config[i]->lib);

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

		pthread_rwlock_init(&config[i]->lock, NULL);

		/* handle growing config */
		if (strncmp(start, "grow;", 5) != 0)
			continue;
		SET_IBUF_START(start);
		grow = (struct grow *) calloc(1, sizeof(struct grow));
		if (!grow) {
			pr_err("Error: No space for grow structure, "
				"ignoring it!\n");
			continue;
		}
		scanned = sscanf(start, "%zd;%zd;%c%u;" SCN_PTR ";"
			SCN_PTR, &grow->size_min, &grow->size_max, &op_ch,
			&grow->add, &grow->code_addr, &grow->stack_offs);
		if (scanned != 6 || op_ch != '+') {
			pr_err("Error: Grow parsing failed!\n");
			free(grow);
			goto err;
		}
		grow->code_offs = grow->code_addr;
		grow->type = GROW_ADD;
		for (j = 5; j > 0; --j)
			SET_IBUF_START(start);
		SET_CODE_LIB(start, grow->lib);
		config[i]->grow = grow;
	}
	if (i != num_cfg)
		goto err;

	/* debug config */
	pr_dbg("num_cfg: %u\n", num_cfg);
	output_cfg();

	wbytes = write(ofd, "ready\n", sizeof("ready\n"));
	if (wbytes < 0)
		perror("write");
	if (read_input(ibuf, sizeof(ibuf) - 1) != 0) {
		pr_err("Couldn't read code offsets!\n");
	} else {
		char *start = ibuf, *pos;
		for (i = 0; i < num_cfg; i++) {
			ptr_t code_offs = 0;
			if (sscanf(start, SCN_PTR ";", &code_offs) < 1) {
				pr_err("Code offset parsing error!\n");
			} else {
				pos = strchr(start, ';');
				if (pos)
					start = pos + 1;
				pr_dbg("code_offs %d: " PRI_PTR "\n",
					i, code_offs);
			}
			if (config[i]->code_addr)
				config[i]->code_addr += code_offs;
			if (config[i]->grow) {
				code_offs = 0;
				if (sscanf(start, SCN_PTR ";",
				    &code_offs) < 1) {
					pr_err("Code offset parsing error!\n");
				} else {
					pos = strchr(start, ';');
					if (pos)
						start = pos + 1;
					pr_dbg("code_offs %dg: " PRI_PTR "\n",
						i, code_offs);
				}
				if (config[i]->grow->code_addr)
					config[i]->grow->code_addr += code_offs;
			}
		}
		pr_out("Config after early PIC/PIE handling:\n");
		output_cfg();
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
		pthread_rwlock_destroy(&config[i]->lock);
}

static inline void store_mem_addr (struct cfg *cfg, ptr_t mem_addr)
{
	u32 j;

	if (pthread_rwlock_wrlock(&cfg->lock) < 0) {
		perror("pthread_rwlock_wrlock");
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
	if (pthread_rwlock_unlock(&cfg->lock) < 0)
		perror("pthread_rwlock_unlock");
out:
	return;
}

void postprocess_malloc (ptr_t ffp, size_t size, ptr_t mem_addr)
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

void preprocess_free (ptr_t mem_addr)
{
	i32 wbytes;
	u32 i, j;

	if (!active || !mem_addr)
		return;

	for (i = 0; config[i] != NULL; i++) {
		if (pthread_rwlock_rdlock(&config[i]->lock) < 0) {
			perror("pthread_rwlock_rdlock");
			goto cont;
		}
		if (!config[i]->mem_addrs)
			goto unlock_cont;
		for (j = 0; config[i]->mem_addrs[j] != ADDR_INVAL; j++) {
			if (config[i]->mem_addrs[j] == mem_addr)
				goto found;
		}
unlock_cont:
		if (pthread_rwlock_unlock(&config[i]->lock) < 0)
			perror("pthread_rwlock_unlock");
cont:
		continue;
found:
		config[i]->mem_addrs[j] = 0;
		if (j < config[i]->insert_idx)
			config[i]->insert_idx = j;
		if (pthread_rwlock_unlock(&config[i]->lock) < 0)
			perror("pthread_rwlock_unlock");
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

/* ############################# */
/* ##### late PIC handling ##### */
/* ############################# */

static inline i32 send_lib_name (const char *lib_name)
{
	i32 wbytes;
	char obuf[BUF_SIZE + 1] = { 0 };

	pr_dbg("PIC: Sending lib_name %s.\n", lib_name);

	wbytes = snprintf(obuf, BUF_SIZE, "l;%s\n", lib_name);
	if (wbytes < 0) {
		perror(PFX "snprintf");
		goto err;
	}
#if DEBUG_MEM
	pr_out("%s", obuf);
#endif
	wbytes = write(ofd, obuf, wbytes);
	if (wbytes < 0) {
		perror("dlopen write");
		goto err;
	}
	return 0;
err:
	return -1;
}

static inline i32 get_lib_load_addr (const char *lib_name, ptr_t *lib_load)
{
	i32 ret;
	char ibuf[BUF_SIZE] = { 0 };
	ptr_t lib_start = 0, lib_end = 0;

	ret = send_lib_name(lib_name);
	if (ret)
		goto err;

	pr_dbg("PIC: Reading load address of %s.\n", lib_name);

	ret = read_input(ibuf, sizeof(ibuf) - 1);
	if (ret) {
		pr_err("PIC: %s: Couldn't read from FIFO!\n", lib_name);
		goto err;
	}
	ret = sscanf(ibuf, SCN_PTR ";" SCN_PTR, &lib_start, &lib_end);
	if (ret < 2) {
		pr_err("PIC: %s: Library load address parsing error!\n",
			lib_name);
		goto err;
	}
	*lib_load = lib_start;

	return 0;
err:
	return -1;
}

/*
 * Inform ugtrain about loading of a library (late PIC handling).
 */
void postprocess_dlopen (const char *lib_path)
{
	i32 ret, i;
	char *lib_name;
	ptr_t lib_load = 0;

	if (!active)
		return;

	lib_name = basename((char *) lib_path);
	if (lib_name[0] == '\0')
		return;

	for (i = 0; config[i] != NULL; i++) {
		struct grow *grow;

		if (!config[i]->lib || !strstr(config[i]->lib, lib_name))
			goto handle_grow;
		if (lib_load) {
			config[i]->code_addr = config[i]->code_offs + lib_load;
			goto handle_grow;
		}
		ret = get_lib_load_addr(lib_name, &lib_load);
		if (ret)
			break;
		if (lib_load) {
			pr_out("PIC: New load address of %s: "
				PRI_PTR "\n", lib_name, lib_load);
			config[i]->code_addr = config[i]->code_offs + lib_load;
		}
handle_grow:
		if (!config[i]->grow)
			continue;
		grow = config[i]->grow;
		if (!grow->lib || !strstr(grow->lib, lib_name))
			continue;
		if (lib_load) {
			grow->code_addr = grow->code_offs + lib_load;
			continue;
		}
		ret = get_lib_load_addr(lib_name, &lib_load);
		if (ret)
			break;
		if (lib_load) {
			pr_out("PIC: New load address of %s: "
				PRI_PTR "\n", lib_name, lib_load);
			grow->code_addr = grow->code_offs + lib_load;
		}
	}
}
