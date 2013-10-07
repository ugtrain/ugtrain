#!/bin/bash

# The issue is that code address and stack offsets for mallocs can differ
# between distributions, compilers and game versions. But the thing which
# remains constant is often the way how the code internally works.

# Tested with: Warzone 2100 2.3.8

# We already know that warzone2100 is a 32-bit C++ application and that
# the DROID structure is allocated with malloc().
# From previous discovery runs we already know the malloc size (876 or
# 0x36c) for the object (DROID) in which the BODY POINTS are stored.

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"
APP_VERS=`${APP_PATH} --version | grep -o "Version.*" | cut -d ' ' -f 2`

IFS=`printf '\n+'`
CODE_PART=`objdump -D "$APP_PATH" | grep "\<malloc@plt\>" -B 1 -A 1 | grep -A 2 0x36c`
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

RESULT=`echo "1;Droid;0x$CODE_ADDR"`
echo "$RESULT"

# Should return something like this:
# 813ce5e:	c7 04 24 6c 03 00 00 	movl   $0x36c,(%esp)
# 813ce65:	e8 76 73 f3 ff       	call   80741e0 <malloc@plt>
# 813ce6a:	85 c0                	test   %eax,%eax

# This shows us that 0x813ce6a is the relevant code address.

# We can jump directly to stage 4 of the discovery with that and leave the
# heap start and end offsets at 0x0 (NULL) as we already know the unique
# code address and malloc size.
