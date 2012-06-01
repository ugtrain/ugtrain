bin_PROGRAMS = ugtrain
ugtrain_SOURCES = cfgentry.h \
                  getch.h \
                  parser.h \
                  ugtrain.cpp \
                  getch_l.c \
                  parser.cpp

ugtrain_LDADD = $(DEPS_LIBS) ../libgcheater/libgcheater.la
AM_CPPFLAGS = $(DEPS_CPPFLAGS) \
              -I../libgcheater
