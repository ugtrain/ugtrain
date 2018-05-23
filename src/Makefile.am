SUBDIRS = linuxhooking tools

bin_PROGRAMS = ugtrain

# use "ls -1 *.c*" for an ordered list
ugtrain_SOURCES = \
	adaption.cpp \
	aslr.cpp \
	cfgoutput.cpp \
	cfgparser.cpp \
	common.cpp \
	control.cpp \
	discovery.cpp \
	dump.cpp \
	fifoparser.cpp \
	memmgmt.cpp \
	options.cpp \
	ugtrain.cpp \
	valoutput.cpp
if LINUX
ugtrain_SOURCES += \
	lib/getch_l.c \
	lib/maps_l.c \
	lib/preload_l.c \
	lib/system_l.c
else
ugtrain_SOURCES += \
	lib/getch_w.c \
	lib/maps_w.c \
	lib/system_w.c
endif

ugtrain_LDADD = $(DEPS_LIBS)
AM_CFLAGS = $(DEPS_CFLAGS) -Wall
if HAVE_PROCMEM
AM_CFLAGS += -DHAVE_PROCMEM=1
endif
if TESTING
AM_CFLAGS += -DTESTING=1
endif
# use the same compiler flags for C and C++
# (required for hardening builds)
AM_CXXFLAGS = $(AM_CFLAGS) $(CFLAGS)

if ENABLE_BASH_COMPLETION
bashcompletiondir = $(BASH_COMPLETION_DIR)
dist_bashcompletion_DATA = bash_completion/ugtrain
endif
