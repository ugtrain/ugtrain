SUBDIRS = linuxhooking tools

bin_PROGRAMS = ugtrain
ugtrain_SOURCES = common.h \
                  options.h \
                  cfgentry.h \
                  getch.h \
                  preload.h \
                  system.h \
                  cfgparser.h \
                  cfgoutput.h \
                  fifoparser.h \
                  control.h \
                  dump.h \
                  memmgmt.h \
                  valoutput.h \
                  discovery.h \
                  adaption.h \
                  commont.cpp \
                  common.cpp \
                  ugtrain.cpp \
                  options.c \
                  cfgparser.cpp \
                  cfgoutput.cpp \
                  getch_l.c \
                  getch_w.c \
                  preload_l.c \
                  system_l.c \
                  system_w.c \
                  memattach_l.c \
                  memattach_w.c \
                  fifoparser.cpp \
                  control.cpp \
                  dump.cpp \
                  memmgmt.cpp \
                  valoutput.cpp \
                  discovery.cpp \
                  adaption.cpp

ugtrain_LDADD = $(DEPS_LIBS)
AM_CPPFLAGS = $(DEPS_CPPFLAGS) \
              -Wall
