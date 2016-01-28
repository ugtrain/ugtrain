/* memhooks.c:    hooking memory allocation functions
 *
 * Copyright (c) 2012..2016 Sebastian Parschauer <s.parschauer@gmx.de>
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
#include <limits.h>     /* PIPE_BUF */
#include <stdlib.h>     /* malloc, calloc, realloc, free */
#include <dlfcn.h>      /* dlsym, dlopen */
#ifdef HAVE_GLIB
#include <glib.h>       /* g_malloc */
#endif

/* local includes */
#include <common.h>

/* Malloc hooks */
#define HOOK_MALLOC 1
#define HOOK_CALLOC 1
#define HOOK_REALLOC 1
#define HOOK_FREE 1
/* late PIC hook */
#define HOOK_DLOPEN 1
/* GLIB hooks */
#define HOOK_G_MALLOC 1
#define HOOK_G_MALLOC0 1
#define HOOK_G_MALLOC_N 1
#define HOOK_G_MALLOC0_N 1
#define HOOK_G_FREE 1
#define HOOK_G_SLICE_ALLOC 1
#define HOOK_G_SLICE_ALLOC0 1
#define HOOK_G_SLICE_FREE1 1

/*
 * Ask gcc for the current stack frame pointer.
 * We don't use the stack pointer as we are not interested in the
 * stuff we have ourselves on the stack and for arch independence.
 */
#ifndef FIRST_FRAME_POINTER
#define FIRST_FRAME_POINTER  __builtin_frame_address(0)
#endif


/* Hooking control (avoid recursion)
 *
 * For details see:
 * http://www.slideshare.net/tetsu.koba/tips-of-malloc-free
 */
static __thread bool no_hook = false;


/* malloc hooking interfaces */
extern void postprocess_malloc (ptr_t ffp, size_t size, ptr_t mem_addr);
extern void preprocess_free (ptr_t mem_addr);
extern void postprocess_dlopen (const char *lib_path);

/* get e.g. the original malloc() as __orig() */
#define GET_DLSYM(__name_str)						\
	*(void **)(&__orig) = dlsym(RTLD_NEXT, __name_str)

/* universal function hooking template */
#define __HOOK_FUNCTION(__type, __name, __name_str, __params,		\
			__raw_params, __vars, __ret1, __ret2,		\
			__no_hook_code, __pre_code, __post_code)	\
__type __name (__params)						\
{									\
	__vars								\
	static void *(*__orig)(__params) = NULL;			\
									\
	if (no_hook) {							\
		if (!__orig) {						\
			__no_hook_code					\
		}							\
		__ret1 __orig(__raw_params);				\
		return __ret2;						\
	}								\
									\
	/* get the original libc function */				\
	no_hook = true;							\
	__pre_code							\
									\
	if (!__orig)							\
		GET_DLSYM(__name_str);					\
									\
	__ret1 __orig(__raw_params);					\
									\
	__post_code							\
	no_hook = false;						\
									\
	return __ret2;							\
}

/* __HOOK_FUNCTION() with default __no_hook_code */
#define _HOOK_FUNCTION(__type, __name, __name_str, __params,		\
		       __raw_params, __vars, __ret1, __ret2,		\
		       __pre_code, __post_code)				\
	__HOOK_FUNCTION(						\
		__type, __name, __name_str, __params,			\
		__raw_params, __vars, __ret1, __ret2,			\
		GET_DLSYM(__name_str);,					\
		__pre_code, __post_code					\
	)

/* hook any function with malloc() type and parameters */
#define HOOK_MALLOC_FUNCTION(__name, __name_str)			\
	_HOOK_FUNCTION(							\
		void *, __name, __name_str, size_t size, size,		\
		ptr_t ffp = (ptr_t)FIRST_FRAME_POINTER;			\
		void *mem_addr;,					\
		mem_addr =, mem_addr,					\
		/* no pre code */,					\
		postprocess_malloc(ffp, size, (ptr_t)mem_addr);		\
	)

/* void *malloc (size_t size); */
/* void *calloc (size_t nmemb, size_t size); */
/* void *realloc (void *ptr, size_t size); */
/* void free (void *ptr); */

#ifdef HOOK_MALLOC
HOOK_MALLOC_FUNCTION(malloc, "malloc")
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

	mem_addr = (void *)(stat_calloc_space + offs);
	offs += size;

	if (offs >= sizeof(stat_calloc_space)) {
		offs = sizeof(stat_calloc_space);
		return NULL;
	}
	return mem_addr;
}

#define CALLOC_PARAMS		size_t nmemb, size_t size
#define CALLOC_RAW_PARAMS	nmemb, size
__HOOK_FUNCTION(
	void *, calloc, "calloc", CALLOC_PARAMS, CALLOC_RAW_PARAMS,
	ptr_t ffp = (ptr_t)FIRST_FRAME_POINTER;
	void *mem_addr;
	size_t full_size = nmemb * size;
	static void *stat_addr = NULL;,
	mem_addr =, mem_addr,
	if (stat_addr) {
		GET_DLSYM("calloc");
	} else {
		stat_addr = stat_calloc(full_size);
		return stat_addr;
	}, /* no pre code */,
	postprocess_malloc(ffp, full_size, (ptr_t)mem_addr);
)
#endif

#ifdef HOOK_REALLOC
#define REALLOC_PARAMS		void *ptr, size_t size
#define REALLOC_RAW_PARAMS	ptr, size
__HOOK_FUNCTION(
	void *, realloc, "realloc", REALLOC_PARAMS, REALLOC_RAW_PARAMS,
	ptr_t ffp = (ptr_t)FIRST_FRAME_POINTER;
	void *mem_addr;,
	mem_addr =, mem_addr,
	GET_DLSYM("realloc");,
	preprocess_free((ptr_t)ptr);,
	postprocess_malloc(ffp, size, (ptr_t)mem_addr);
)
#endif

#ifdef HOOK_FREE
_HOOK_FUNCTION(
	void, free, "free", void *ptr, ptr,
	/* no vars */,;,;,
	preprocess_free((ptr_t)ptr);,
	/* no post code */
)
#endif

/* late PIC handling */
#ifdef HOOK_DLOPEN
#define DLOPEN_PARAMS		const char *filename, int flag
#define DLOPEN_RAW_PARAMS	filename, flag
__HOOK_FUNCTION(
	void *, dlopen, "dlopen", DLOPEN_PARAMS, DLOPEN_RAW_PARAMS,
	void *handle;,
	handle =, handle,
	GET_DLSYM("dlopen");,
	/* no pre code */,
	postprocess_dlopen(filename);
)
#endif


/* Glib hooks */
#ifdef HAVE_GLIB

/* hook any function with g_malloc() type and parameters */
#define HOOK_G_MALLOC_FUNCTION(__name, __name_str)			\
	_HOOK_FUNCTION(							\
		gpointer, __name, __name_str, gsize n_bytes, n_bytes,	\
		ptr_t ffp = (ptr_t)FIRST_FRAME_POINTER;			\
		gpointer mem_addr;,					\
		mem_addr =, mem_addr,					\
		/* no pre code */,					\
		postprocess_malloc(ffp, n_bytes, (ptr_t)mem_addr);	\
	)

/* gpointer g_malloc (gsize n_bytes); */
/* gpointer g_malloc0 (gsize n_bytes); */
/* gpointer g_malloc_n (gsize n_blocks, gsize n_block_bytes) */
/* gpointer g_malloc0_n (gsize n_blocks, gsize n_block_bytes) */
/* void g_free (gpointer mem); */
/* gpointer g_slice_alloc (gsize block_size); */
/* gpointer g_slice_alloc0 (gsize block_size); */
/* gpointer g_slice_free1 (gsize block_size, gpointer mem_block); */

#ifdef HOOK_G_MALLOC
HOOK_G_MALLOC_FUNCTION(g_malloc, "g_malloc")
#endif

#ifdef HOOK_G_MALLOC0
HOOK_G_MALLOC_FUNCTION(g_malloc0, "g_malloc0")
#endif

#define G_MALLOC_N_PARAMS	gsize n_blocks, gsize n_block_bytes
#define G_MALLOC_N_RAW_PARAMS	n_blocks, n_block_bytes

#ifdef HOOK_G_MALLOC_N
__HOOK_FUNCTION(
	gpointer, g_malloc_n, "g_malloc_n", G_MALLOC_N_PARAMS,
	G_MALLOC_N_RAW_PARAMS,
	ptr_t ffp = (ptr_t)FIRST_FRAME_POINTER;
	gpointer mem_addr;,
	mem_addr =, mem_addr,
	GET_DLSYM("g_malloc_n");,
	/* no pre code */,
	postprocess_malloc(ffp, n_blocks * n_block_bytes, (ptr_t)mem_addr);
)
#endif

#ifdef HOOK_G_MALLOC0_N
__HOOK_FUNCTION(
	gpointer, g_malloc0_n, "g_malloc0_n", G_MALLOC_N_PARAMS,
	G_MALLOC_N_RAW_PARAMS,
	ptr_t ffp = (ptr_t)FIRST_FRAME_POINTER;
	gpointer mem_addr;,
	mem_addr =, mem_addr,
	GET_DLSYM("g_malloc0_n");,
	/* no pre code */,
	postprocess_malloc(ffp, n_blocks * n_block_bytes, (ptr_t)mem_addr);
)
#endif

#ifdef HOOK_G_FREE
_HOOK_FUNCTION(
	void, g_free, "g_free", gpointer mem, mem,
	/* no vars */,;,;,
	preprocess_free((ptr_t)mem);,
	/* no post code */
)
#endif

#ifdef HOOK_G_SLICE_ALLOC
HOOK_G_MALLOC_FUNCTION(g_slice_alloc, "g_slice_alloc")
#endif

#ifdef HOOK_G_SLICE_ALLOC0
HOOK_G_MALLOC_FUNCTION(g_slice_alloc0, "g_slice_alloc0")
#endif

#ifdef HOOK_G_SLICE_FREE1
#define G_SLICE_FREE1_PARAMS		gsize block_size, gpointer mem_block
#define G_SLICE_FREE1_RAW_PARAMS	block_size, mem_block
__HOOK_FUNCTION(
	void, g_slice_free1, "g_slice_free1", G_SLICE_FREE1_PARAMS,
	G_SLICE_FREE1_RAW_PARAMS,
	/* no vars */,;,;,
	GET_DLSYM("g_slice_free1");,
	preprocess_free((ptr_t)mem_block);,
	/* no post code */
)
#endif

#endif /* HAVE_GLIB */
