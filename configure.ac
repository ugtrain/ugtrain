#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([ugtrain], [0.2.0-pre], [ugtrain-devel@lists.sourceforge.net])

AC_CONFIG_MACRO_DIR(m4)
AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE()
AM_MAINTAINER_MODE

AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

LIB_CURRENT=0
LIB_REVERSION=0
LIB_AGE=0

VERSION_INFO="$LIB_CURRENT:$LIB_REVERSION:$LIB_AGE"
AC_SUBST(VERSION_INFO)

# --enable-XX options
AC_ARG_ENABLE([multilib],
  [  --enable-multilib  enable multilib support],
  [], [enable_multilib=no])
AC_ARG_ENABLE([glib],
  [  --enable-glib  enable GLIB (g_malloc hooking) support],
  [], [enable_glib=no])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_STDC

# Check for 64-bit CPU
bits64="no"
case $target_cpu in
  x86_64)
    bits64="yes"
    ;;
esac

# Test multilib compile
if test "$enable_multilib" != "no" -a bits64 != "no"; then
  TMP_CC="$CC"
  CC="$CC -m32"
  AC_MSG_CHECKING([whether $CC works])
  AC_LINK_IFELSE([AC_LANG_PROGRAM()],AC_MSG_RESULT([yes]),
		 [AC_MSG_RESULT([no])
		  AC_MSG_ERROR([Cannot build a 32-bit program, you need \
				to install 32-bit development libraries.])])
  CC="$TMP_CC"
fi
# Test GTK compile
AS_IF([test "$enable_glib" != "no"], [
   PKG_CHECK_MODULES([GLIB], [glib-2.0])
   AC_SUBST([GTK_CFLAGS])
   AC_SUBST([GTK_LIBS])])
AM_CONDITIONAL([LINUX], test "$target_os" = "linux-gnu")
AM_CONDITIONAL([MULTILIB], test "$enable_multilib" != "no")
AM_CONDITIONAL([HAVE_GLIB], test "$enable_glib" != "no")
AM_CONDITIONAL([BITS_32], test "$bits64" = "no" -o "$enable_multilib" != "no")
AM_CONDITIONAL([BITS_64], test "$bits64" != "no")

# Checks for pkg-config.
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(stdlib.h string.h unistd.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T

# Checks for library functions.
AC_FUNC_STAT
AC_CHECK_FUNCS(ptrace memcpy memcmp getenv setenv)

AC_CHECK_HEADER(windows.h)

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	src/linuxhooking/Makefile
	src/tools/Makefile])
AC_OUTPUT
