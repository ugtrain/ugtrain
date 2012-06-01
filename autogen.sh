#!/bin/sh
mkdir m4 2>/dev/null
touch ltmain.sh 2>/dev/null
aclocal || exit 1
autoconf || exit 1
automake --gnu --add-missing || exit 1
autoreconf -i || exit 1
cp `which libtool` .
