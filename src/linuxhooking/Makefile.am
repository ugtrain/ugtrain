if LINUX

if BITS_64
if MULTILIB
lib_LTLIBRARIES = libmemdisc32.la libmemhack32.la \
		  libmemdisc64.la libmemhack64.la
else
lib_LTLIBRARIES = libmemdisc64.la libmemhack64.la
endif
else
lib_LTLIBRARIES = libmemdisc32.la libmemhack32.la
endif

include_HEADERS =

if BITS_32
libmemdisc32_la_SOURCES = libmemdisc.c libcommon.c ../system_l.c
libmemhack32_la_SOURCES = libmemhack.c libcommon.c ../system_l.c
if HAVE_GLIB
libmemdisc32_la_CFLAGS = $(DEPS_CFLAGS) -m32 -Wall -DHAVE_GLIB=1 @GTK_CFLAGS@
libmemhack32_la_CFLAGS = $(DEPS_CFLAGS) -m32 -Wall -DHAVE_GLIB=1 @GTK_CFLAGS@
libmemdisc32_la_LIBADD = $(DEPS_LIBS) @GTK_LIBS@
libmemhack32_la_LIBADD = $(DEPS_LIBS) @GTK_LIBS@
else
libmemdisc32_la_CFLAGS = $(DEPS_CFLAGS) -m32 -Wall
libmemhack32_la_CFLAGS = $(DEPS_CFLAGS) -m32 -Wall
libmemdisc32_la_LIBADD = $(DEPS_LIBS)
libmemhack32_la_LIBADD = $(DEPS_LIBS)
endif
libmemdisc32_la_LDFLAGS = -ldl
libmemhack32_la_LDFLAGS = -ldl
endif

if BITS_64
libmemdisc64_la_SOURCES = libmemdisc.c libcommon.c ../system_l.c
libmemhack64_la_SOURCES = libmemhack.c libcommon.c ../system_l.c
if HAVE_GLIB
libmemdisc64_la_CFLAGS = $(DEPS_CFLAGS) -m64 -Wall -DHAVE_GLIB=1 @GTK_CFLAGS@
libmemhack64_la_CFLAGS = $(DEPS_CFLAGS) -m64 -Wall -DHAVE_GLIB=1 @GTK_CFLAGS@
libmemdisc64_la_LIBADD = $(DEPS_LIBS) @GTK_LIBS@
libmemhack64_la_LIBADD = $(DEPS_LIBS) @GTK_LIBS@
else
libmemdisc64_la_CFLAGS = $(DEPS_CFLAGS) -m64 -Wall
libmemhack64_la_CFLAGS = $(DEPS_CFLAGS) -m64 -Wall
libmemdisc64_la_LIBADD = $(DEPS_LIBS)
libmemhack64_la_LIBADD = $(DEPS_LIBS)
endif
libmemdisc64_la_LDFLAGS = -ldl
libmemhack64_la_LDFLAGS = -ldl
endif

endif
