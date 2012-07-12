#!/bin/bash
rm -Rf scanmem
svn co http://scanmem.googlecode.com/svn/trunk/ -r 251 scanmem
patch -p1 < 001_*.patch
