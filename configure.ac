#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([ugtrain], [0.2.1-pre], [ugtrain-devel@lists.sourceforge.net])

AC_CONFIG_MACRO_DIR(m4)
AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE([subdir-objects])
AM_MAINTAINER_MODE

AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

# --enable-XX options
AC_ARG_ENABLE([multilib],
  [  --enable-multilib  enable multilib support],
  [], [enable_multilib=no])
AC_ARG_ENABLE([glib],
  [  --enable-glib  enable GLIB (g_malloc hooking) support],
  [], [enable_glib=no])

# Checks for programs.
AC_PROG_CXX([g++])
AC_LANG_PUSH([C++])
AC_MSG_CHECKING([if the C++ compiler can create executables])
AC_TRY_LINK([], [return 0;], AC_MSG_RESULT([yes]), AC_MSG_ERROR([no]))
AC_LANG_POP([C++])
AC_PROG_CC
AM_PROG_CC_STDC
AM_PROG_CC_C_O

# check compiler flags
AC_LANG_PUSH([C++])
CXXFLAGS="-m32"
AC_MSG_CHECKING([whether the C++ compiler accepts -m32])
AC_COMPILE_IFELSE([AC_LANG_SOURCE(
  [[long test;]])],
    [has_m32="yes"; echo yes],
    [has_m32="no"; echo no])
CXXFLAGS="-m64"
AC_MSG_CHECKING([whether the C++ compiler accepts -m64])
AC_COMPILE_IFELSE([AC_LANG_SOURCE(
  [[long test;]])],
    [has_m64="yes"; echo yes],
    [has_m64="no"; echo no])
CXXFLAGS=""
AC_LANG_POP([C++])

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
# Test GLIB compile
AS_IF([test "$enable_glib" != "no"], [
   PKG_CHECK_MODULES([GLIB], [glib-2.0])
   AC_SUBST([GLIB_CFLAGS])
   AC_SUBST([GLIB_LIBS])])
AM_CONDITIONAL([LINUX], test `echo "$target_os" | grep -o "linux"` = "linux")
AM_CONDITIONAL([MULTILIB], test "$enable_multilib" != "no")
AM_CONDITIONAL([HAVE_GLIB], test "$enable_glib" != "no")
AM_CONDITIONAL([BITS_32], test "$bits64" = "no" -o "$enable_multilib" != "no")
AM_CONDITIONAL([BITS_64], test "$bits64" != "no")
AM_CONDITIONAL([HAVE_M32], test "$has_m32" != "no")
AM_CONDITIONAL([HAVE_M64], test "$has_m64" != "no")

AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(stdlib.h string.h unistd.h)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINTPTR_T

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
