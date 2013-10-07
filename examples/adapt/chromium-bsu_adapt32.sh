#!/bin/bash

# The issue is that code address and stack offsets for mallocs can differ
# between distributions, compilers and game versions. But the thing which
# remains constant is often the way how the code internally works.

# Tested with: chromium-bsu 0.9.14.1, 0.9.15, 0.9.15.1

# We already know that chromium-bsu is a 32-bit C++ application. Therefore,
# objects are allocated most likely by the "_Znwj" function. This function
# calls malloc internally.
# And from previous discovery runs we already know the malloc size (272 or
# 0x110) for the object in which the NUMBER OF LIVES is stored.

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"

IFS=`printf '\n+'`
CODE_PART=`objdump -D "$APP_PATH" | grep "_Znwj" -B 2 -A 1 | grep -A 2 0x110`
if [ "$CODE_PART" == "" ]; then exit 1; fi

CODE_LINES=`echo -e "$CODE_PART" | wc -l`
if [ $CODE_LINES -ne 3 ]; then exit 1; fi

CODE_CALL=`echo -e "$CODE_PART" | cut -d '
' -f 2 | grep call`
if [ "$CODE_CALL" == "" ]; then exit 1; fi

CODE_ADDR=`echo -e "$CODE_PART" | cut -d '
' -f 3 | cut -d ':' -f 1 | tr -d [:blank:]`
if [ "$CODE_ADDR" == "" ]; then exit 1; fi
IFS=''

RESULT=`echo "1;HeroAircraft;0x$CODE_ADDR"`
echo "$RESULT"

# Should return something like this:
#  805720d:	c7 04 24 10 01 00 00 	movl   $0x110,(%esp)
#  8057214:	e8 db 36 ff ff       	call   804a8f4 <_Znwj@plt>
#  8057219:	89 c3                	mov    %eax,%ebx

# This shows us that 0x8057219 is the relevant code address.

# We can jump directly to stage 4 of the discovery with that and leave the
# heap start and end offsets at 0x0 (NULL) as we already know the unique
# code address and malloc size.
