#!/bin/sh
# Run this to generate all the initial makefiles, etc.

# code taken from Warzone 2100 3.1.1 as they also use
# GPLv2 and have it implemented already very well

DIE=0
SRCDIR=`dirname $0`
BUILDDIR=`pwd`
SRCFILE=src/ugtrain.cpp
PACKAGE=ugtrain

debug ()
# print out a debug message if DEBUG is a defined variable
{
  if [ ! -z "$DEBUG" ]; then
    echo "DEBUG: $1"
  fi
}

set_and_check_symlink ()
# set a symlink and check if it works afterwards
{
  local tgt="$1"
  local name="$2"
  echo -n "+ setting symlink $name/ to $tgt/ ... "
  ln -sfT "$tgt" "$name" 2>/dev/null

  if [ ! -d "$name" ]; then
    echo "failed."
    return 1
  fi

  cd "$name" 2>/dev/null
  local rc=$?
  if [ $rc -ne 0 ]; then
    rm "$name"
    echo "failed."
    return 1
  fi

  cd ..
  echo "ok."
}

machine_check ()
# check which debian directory is required and set a symlink to it
{
  local deb="debian"
  local deb_harmattan="debian_harmattan"
  local deb_pc="debian_pc"
  local machine="`uname -m`"
  if [ -z "$machine" ]; then
    echo "not found."
    return 1
  fi
  echo "$machine."

  case "$machine" in
  "arm")
    local aegis_manifest="`which aegis-manifest`"
    if [ -n "$aegis_manifest" ]; then
      echo "Meego 1.2 Harmattan detected."
      set_and_check_symlink "$deb_harmattan" "$deb"
    fi
    ;;
  "i686"|"x86_64")
    set_and_check_symlink "$deb_pc" "$deb"
    ;;
  *)
    ;;
  esac
}

version_check ()
# check the version of a package
# first argument : complain ('1') or not ('0')
# second argument : package name (executable)
# third argument : source download url
# rest of arguments : major, minor, micro version
{
  local complain=$1
  local package=$2
  local url=$3
  local major=$4
  local minor=$5
  local micro=$6

  local wrong=

  debug "major $major minor $minor micro $micro"
  local version=$major
  if [ ! -z "$minor" ]; then version=$version.$minor; else minor=0; fi
  if [ ! -z "$micro" ]; then version=$version.$micro; else micro=0; fi

  debug "version $version"
  echo -n "+ checking for $package >= $version ... "

  ($package --version) < /dev/null > /dev/null 2>&1 ||
  {
    echo
    echo "You must have $package installed to compile $PACKAGE."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at $url"
    return 1
  }
  # the following line is carefully crafted sed magic
  local pkg_version=`$package --version|head -n 1|\
    sed 's/([^)]*)//g;s/^[a-zA-Z\.\ \-\/]*//;s/ .*$//'`
  debug "pkg_version $pkg_version"
  local pkg_major=`echo $pkg_version | cut -d. -f1`
  local pkg_minor=`echo $pkg_version | sed s/[-,a-z,A-Z].*// | cut -d. -f2`
  local pkg_micro=`echo $pkg_version | sed s/[-,a-z,A-Z].*// | cut -d. -f3`
  [ -z "$pkg_minor" ] && pkg_minor=0
  [ -z "$pkg_micro" ] && pkg_micro=0

  debug "found major $pkg_major minor $pkg_minor micro $pkg_micro"

  #start checking the version
  if [ "$pkg_major" -lt "$major" ]; then
    wrong=1
  elif [ "$pkg_major" -eq "$major" ]; then
    if [ "$pkg_minor" -lt "$minor" ]; then
      wrong=1
    elif [ "$pkg_minor" -eq "$minor" -a "$pkg_micro" -lt "$micro" ]; then
      wrong=1
    fi
  fi

  if [ ! -z "$wrong" ]; then
   echo "found $pkg_version, not ok !"
   if [ "$complain" -eq "1" ]; then
     echo
     echo "You must have $package $version or greater to compile $PACKAGE."
     echo "Get the latest version from <$url>."
     echo
   fi
   return 1
  else
    echo "found $pkg_version, ok."
  fi
}

# Chdir to the srcdir, then run auto* tools.
cd "$SRCDIR"

echo -n "+ checking machine type ... "
machine_check

version_check 1 "autoconf" "ftp://ftp.gnu.org/pub/gnu/autoconf/" 2 56 || DIE=1
version_check 1 "automake" "ftp://ftp.gnu.org/pub/gnu/automake/" 1 8 || DIE=1
if [ "$DIE" -eq 1 ]; then
  exit 1
fi

[ -f "$SRCFILE" ] || {
  echo "Are you sure $SRCDIR is a valid source directory?"
  exit 1
}

echo "+ running libtoolize ..."
libtoolize || {
  echo
  echo "libtoolize failed - check that it is present on system"
  exit 1
}
echo "+ running aclocal ..."
aclocal -I m4 || {
  echo
  echo "aclocal failed - check that all needed development files"\
       "are present on system"
  exit 1
}
#echo "+ running autoheader ... "
#autoheader || {
#  echo
#  echo "autoheader failed"
#  exit 1
#}
echo "+ running autoconf ... "
autoconf || {
  echo
  echo "autoconf failed"
  exit 1
}
echo "+ running automake ... "
automake -a -c --foreign || {
  echo
  echo "automake failed"
  exit 1
}

# Chdir back to the builddir before the configure step.
cd "$BUILDDIR"

# now remove the cache, because it can be considered dangerous in this case
echo "+ removing config.cache ... "
rm -f config.cache

echo
echo "Now type '$SRCDIR/configure && make' to compile."

exit 0
