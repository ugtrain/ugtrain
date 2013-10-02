#!/bin/bash

# The issue is that code address and stack offsets for mallocs can differ
# between distributions, compilers and game versions. But the thing which
# remains constant is often the way how the code internally works.

# Tested with: Warzone 2100 3.1.0

# We already know that warzone2100 is a 64-bit C++ application and that
# the DROID and Structure classes are allocated with _Znwm().
# From previous discovery runs we already know the malloc size (864 or
# 0x360) for the object (DROID) in which the BODY POINTS are stored. The
# Structure object has the malloc size 424 or 0x1a8.

CWD=`dirname $0`
cd "$CWD"
APP_PATH=`which warzone2100`
APP_VERS=`${APP_PATH} --version | grep -o "Version.*" | cut -d ' ' -f 2`

IFS=`printf '\n+'`
CODE_PART=`objdump -D "$APP_PATH" | grep "\<_Znwm@plt\>" -B 2 -A 1 | grep -A 3 0x360`
if [ "$CODE_PART" == "" ]; then exit 1; fi

CODE_LINES=`echo -e "$CODE_PART" | wc -l`
if [ $CODE_LINES -ne 4 ]; then exit 1; fi

CODE_CALL=`echo -e "$CODE_PART" | cut -d '
' -f 3 | grep call`
if [ "$CODE_CALL" == "" ]; then exit 1; fi

CODE_ADDR=`echo -e "$CODE_PART" | cut -d '
' -f 4 | cut -d ':' -f 1 | tr -d [:blank:]`
if [ "$CODE_ADDR" == "" ]; then exit 1; fi

############################################

CODE_PART2=`objdump -D "$APP_PATH" | grep "\<_Znwm@plt\>" -B 2 -A 1 | grep -A 3 0x1a8 | tail -n 4`
if [ "$CODE_PART2" == "" ]; then exit 1; fi

CODE_LINES2=`echo -e "$CODE_PART2" | wc -l`
if [ $CODE_LINES2 -ne 4 ]; then exit 1; fi

CODE_CALL2=`echo -e "$CODE_PART2" | cut -d '
' -f 3 | grep call`
if [ "$CODE_CALL2" == "" ]; then exit 1; fi

CODE_ADDR2=`echo -e "$CODE_PART2" | cut -d '
' -f 4 | cut -d ':' -f 1 | tr -d [:blank:]`
if [ "$CODE_ADDR2" == "" ]; then exit 1; fi
IFS=''

RESULT=`echo "2;Droid;0x$CODE_ADDR;Structure;0x$CODE_ADDR2"`
echo "$RESULT"

# Should return something like this:
#  4a6c4d:       bf 60 03 00 00          mov    $0x360,%edi
#  4a6c52:       41 89 c6                mov    %eax,%r14d
#  4a6c55:       e8 b6 8a fb ff          callq  45f710 <_Znwm@plt>
#  4a6c5a:       89 ea                   mov    %ebp,%edx

# This shows us that 0x4a6c5a is the relevant Droid code address.

# We can jump directly to stage 4 of the discovery with that and leave the
# heap start and end offsets at 0x0 (NULL) as we already know the unique
# code address and malloc size per memory class.
