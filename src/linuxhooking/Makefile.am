if LINUX

LIB_VERSION = "0:0:0"

if BITS_64
lib_LTLIBRARIES = libmemdisc64.la libmemhack64.la
if MULTILIB
lib_LTLIBRARIES += libmemdisc32.la libmemhack32.la
else
endif
else
lib_LTLIBRARIES = libmemdisc32.la libmemhack32.la
endif

if BITS_32
libmemdisc32_la_SOURCES = libcommon.c memhooks.c nethooks.c libmemdisc.c
libmemhack32_la_SOURCES = libcommon.c memhooks.c nethooks.c libmemhack.c
libmemdisc32_la_CFLAGS = $(DEPS_CFLAGS) -Wall -I ..
libmemhack32_la_CFLAGS = $(DEPS_CFLAGS) -Wall -I ..
libmemdisc32_la_LIBADD = $(DEPS_LIBS)
libmemhack32_la_LIBADD = $(DEPS_LIBS)
if HAVE_GLIB
if !MULTILIB
libmemdisc32_la_CFLAGS += -DHAVE_GLIB=1 @GLIB_CFLAGS@
libmemhack32_la_CFLAGS += -DHAVE_GLIB=1 @GLIB_CFLAGS@
libmemdisc32_la_LIBADD += @GLIB_LIBS@
libmemhack32_la_LIBADD += @GLIB_LIBS@
endif
endif
if HAVE_M32
libmemdisc32_la_CFLAGS += -m32
libmemhack32_la_CFLAGS += -m32
endif
libmemdisc32_la_LDFLAGS = -ldl -version-info $(LIB_VERSION)
libmemhack32_la_LDFLAGS = -ldl -version-info $(LIB_VERSION) $(PTHREAD_LIBS)
endif

if BITS_64
libmemdisc64_la_SOURCES = libcommon.c memhooks.c nethooks.c libmemdisc.c
libmemhack64_la_SOURCES = libcommon.c memhooks.c nethooks.c libmemhack.c
libmemdisc64_la_CFLAGS = $(DEPS_CFLAGS) -Wall -I ..
libmemhack64_la_CFLAGS = $(DEPS_CFLAGS) -Wall -I ..
libmemdisc64_la_LIBADD = $(DEPS_LIBS)
libmemhack64_la_LIBADD = $(DEPS_LIBS)
if HAVE_GLIB
libmemdisc64_la_CFLAGS += -DHAVE_GLIB=1 @GLIB_CFLAGS@
libmemhack64_la_CFLAGS += -DHAVE_GLIB=1 @GLIB_CFLAGS@
libmemdisc64_la_LIBADD += @GLIB_LIBS@
libmemhack64_la_LIBADD += @GLIB_LIBS@
endif
if HAVE_M64
libmemdisc64_la_CFLAGS += -m64
libmemhack64_la_CFLAGS += -m64
endif
libmemdisc64_la_LDFLAGS = -ldl -version-info $(LIB_VERSION)
libmemhack64_la_LDFLAGS = -ldl -version-info $(LIB_VERSION) $(PTHREAD_LIBS)
endif

endif
