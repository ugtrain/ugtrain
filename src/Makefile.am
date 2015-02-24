SUBDIRS = linuxhooking tools

bin_PROGRAMS = ugtrain

# use "ls -1 *.h; ls -1 *.c*" for an ordered list
ugtrain_SOURCES = \
	lib/getch.h \
	lib/maps.h \
	lib/memattach.h \
	lib/preload.h \
	lib/system.h \
	lib/types.h \
	lib/getch_l.c \
	lib/getch_w.c \
	lib/maps_l.c \
	lib/mapscb_l.cpp \
	lib/memattach_l.c \
	lib/memattach_w.c \
	lib/preload_l.c \
	lib/system_l.c \
	lib/system_w.c \
	adaption.h \
	aslr.h \
	cfgentry.h \
	cfgoutput.h \
	cfgparser.h \
	common.h \
	control.h \
	discovery.h \
	dump.h \
	fifoparser.h \
	memmgmt.h \
	options.h \
	valoutput.h \
	adaption.cpp \
	aslr.cpp \
	cfgoutput.cpp \
	cfgparser.cpp \
	common.cpp \
	commont.cpp \
	control.cpp \
	discovery.cpp \
	dump.cpp \
	fifoparser.cpp \
	memmgmt.cpp \
	options.c \
	ugtrain.cpp \
	valoutput.cpp

ugtrain_LDADD = $(DEPS_LIBS)
AM_CPPFLAGS = $(DEPS_CPPFLAGS) \
              -Wall -g -O2 -I .
