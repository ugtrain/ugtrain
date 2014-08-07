if LINUX

LIB_VERSION="0:0:0"

if BITS_64
if MULTILIB
lib_LTLIBRARIES = libmemdisc32.la libmemhack32.la libprivacy32.la \
		  libmemdisc64.la libmemhack64.la libprivacy64.la
else
lib_LTLIBRARIES = libmemdisc64.la libmemhack64.la libprivacy64.la
endif
else
lib_LTLIBRARIES = libmemdisc32.la libmemhack32.la libprivacy32.la
endif

include_HEADERS =

if BITS_32
libmemdisc32_la_SOURCES = libmemdisc.c libcommon.c
libmemhack32_la_SOURCES = libmemhack.c libcommon.c
libprivacy32_la_SOURCES = libprivacy.c
libmemdisc32_la_CFLAGS = $(DEPS_CFLAGS) -Wall
libmemhack32_la_CFLAGS = $(DEPS_CFLAGS) -Wall
libprivacy32_la_CFLAGS = $(DEPS_CFLAGS) -Wall
libmemdisc32_la_LIBADD = $(DEPS_LIBS)
libmemhack32_la_LIBADD = $(DEPS_LIBS)
libprivacy32_la_LIBADD = $(DEPS_LIBS)
if HAVE_GLIB
libmemdisc32_la_CFLAGS += -DHAVE_GLIB=1 @GLIB_CFLAGS@
libmemhack32_la_CFLAGS += -DHAVE_GLIB=1 @GLIB_CFLAGS@
libmemdisc32_la_LIBADD += @GLIB_LIBS@
libmemhack32_la_LIBADD += @GLIB_LIBS@
endif
if HAVE_M32
libmemdisc32_la_CFLAGS += -m32
libmemhack32_la_CFLAGS += -m32
libprivacy32_la_CFLAGS += -m32
endif
libmemdisc32_la_LDFLAGS = -ldl -version-info $(LIB_VERSION)
libmemhack32_la_LDFLAGS = -ldl -version-info $(LIB_VERSION)
libprivacy32_la_LDFLAGS = -ldl -version-info $(LIB_VERSION)
endif

if BITS_64
libmemdisc64_la_SOURCES = libmemdisc.c libcommon.c
libmemhack64_la_SOURCES = libmemhack.c libcommon.c
libprivacy64_la_SOURCES = libprivacy.c
libmemdisc64_la_CFLAGS = $(DEPS_CFLAGS) -Wall
libmemhack64_la_CFLAGS = $(DEPS_CFLAGS) -Wall
libprivacy64_la_CFLAGS = $(DEPS_CFLAGS) -Wall
libmemdisc64_la_LIBADD = $(DEPS_LIBS)
libmemhack64_la_LIBADD = $(DEPS_LIBS)
libprivacy64_la_LIBADD = $(DEPS_LIBS)
if HAVE_GLIB
libmemdisc64_la_CFLAGS += -DHAVE_GLIB=1 @GLIB_CFLAGS@
libmemhack64_la_CFLAGS += -DHAVE_GLIB=1 @GLIB_CFLAGS@
libmemdisc64_la_LIBADD += @GLIB_LIBS@
libmemhack64_la_LIBADD += @GLIB_LIBS@
endif
if HAVE_M64
libmemdisc64_la_CFLAGS += -m64
libmemhack64_la_CFLAGS += -m64
libprivacy64_la_CFLAGS += -m64
endif
libmemdisc64_la_LDFLAGS = -ldl -version-info $(LIB_VERSION)
libmemhack64_la_LDFLAGS = -ldl -version-info $(LIB_VERSION)
libprivacy64_la_LDFLAGS = -ldl -version-info $(LIB_VERSION)
endif

endif
