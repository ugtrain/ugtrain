bin_PROGRAMS = ugtrain
ugtrain_SOURCES = common.h \
                  options.h \
                  cfgentry.h \
                  getch.h \
                  cfgparser.h \
                  fifoparser.h \
                  discovery.h \
                  common.cpp \
                  ugtrain.cpp \
                  options.c \
                  cfgparser.cpp \
                  getch_l.c \
                  memattach_l.c \
                  fifoparser.cpp \
                  discovery.cpp

ugtrain_LDADD = $(DEPS_LIBS)
AM_CPPFLAGS = $(DEPS_CPPFLAGS) \
              -Wall
