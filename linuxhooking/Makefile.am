lib_LTLIBRARIES = libmemdisc32.la libmemhack32.la \
                  libmemdisc64.la libmemhack64.la

include_HEADERS =

libmemdisc32_la_SOURCES = libmemdisc.c
libmemhack32_la_SOURCES = libmemhack.c
libmemdisc64_la_SOURCES = libmemdisc.c
libmemhack64_la_SOURCES = libmemhack.c

libmemdisc32_la_CFLAGS = $(DEPS_CFLAGS) -m32 -Wall
libmemhack32_la_CFLAGS = $(DEPS_CFLAGS) -m32 -Wall
libmemdisc64_la_CFLAGS = $(DEPS_CFLAGS) -m64 -Wall
libmemhack64_la_CFLAGS = $(DEPS_CFLAGS) -m64 -Wall

libmemdisc32_la_LIBADD = $(DEPS_LIBS)
libmemhack32_la_LIBADD = $(DEPS_LIBS)
libmemdisc64_la_LIBADD = $(DEPS_LIBS)
libmemhack64_la_LIBADD = $(DEPS_LIBS)

libmemdisc32_la_LDFLAGS =
libmemhack32_la_LDFLAGS =
libmemdisc64_la_LDFLAGS =
libmemhack64_la_LDFLAGS =


bin_PROGRAMS = ugpreload

ugpreload_SOURCES = ugpreload.c

ugpreload_LDADD = $(DEPS_LIBS)

AM_CFLAGS = -Wall
