SUBDIRS = linuxhooking tools

bin_PROGRAMS = ugtrain

# use "ls -1 *.c*" for an ordered list
ugtrain_SOURCES = \
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
if LINUX
ugtrain_SOURCES += \
	lib/getch_l.c \
	lib/maps_l.c \
	lib/memattach_l.c \
	lib/preload_l.c \
	lib/system_l.c
else
ugtrain_SOURCES += \
	lib/getch_w.c \
	lib/memattach_w.c \
	lib/system_w.c
endif

ugtrain_LDADD = $(DEPS_LIBS)
AM_CPPFLAGS = $(DEPS_CPPFLAGS) \
              -Wall -g -O2 -I .
if TESTING
AM_CPPFLAGS += -DTESTING=1
endif

if ENABLE_BASH_COMPLETION
bashcompletiondir = $(BASH_COMPLETION_DIR)
dist_bashcompletion_DATA = bash_completion/ugtrain
endif
