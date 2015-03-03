#!/bin/sh

# Tested with: sauerbraten 0.0.20100728.dfsg, 0.0.20130203.dfsg, 0.0.20140302-1

# We already know that sauerbraten is a 64-bit C++ application. Therefore,
# objects are allocated most likely by the "_Znwm" function. This function
# calls malloc internally. But in latest versions malloc is used directly.
# And from previous discovery runs we already know the malloc size for the
# FPSent object (first person shooter entity).

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"
APP_PATH1="/usr/lib/games/sauerbraten/sauer_client"
APP_PATH2="/usr/lib/cube2/cube2_client"
PATH_RESULT=""
RC=0
MSIZE1="0x598"
MSIZE2="0x5b0"

. ./_common_adapt.sh

# check for changed paths
if [ ! -f "$APP_PATH" ]; then
    if [ -f "$APP_PATH1" ]; then
        APP_PATH="$APP_PATH1"
    elif [ -f "$APP_PATH2" ]; then
        APP_PATH="$APP_PATH2"
    else
        echo "$PFX $APP_PATH does not exist!" 1>&2; exit 1
    fi
    PROC_NAME=`basename "$APP_PATH"`
    PATH_RESULT="proc_name;$PROC_NAME;game_binpath;$APP_PATH;"
fi

# try 0.0.20100728.dfsg
get_malloc_code "$APP_PATH" "\<_Znwm@plt\>" "$MSIZE1," 5 15 11
MSIZE="$MSIZE1"
if [ $RC -ne 0 ]; then
    # try 0.0.20130203.dfsg or 0.0.20140302-1
    get_malloc_code "$APP_PATH" "\<malloc@plt\>" "$MSIZE2," 7 31 23
    if [ $RC -ne 0 ]; then exit 1; fi
    MSIZE="$MSIZE2"
fi

RESULT=`echo "1;${PATH_RESULT}FPSent;$MSIZE;0x$CODE_ADDR"`
echo "$RESULT"

# Should return something like this:
# 5e31c2:       bf b0 05 00 00          mov    $0x5b0,%edi
# 5e31c7:       41 54                   push   %r12
# 5e31c9:       55                      push   %rbp
# 5e31ca:       53                      push   %rbx
# 5e31cb:       48 83 ec 08             sub    $0x8,%rsp
# 5e31cf:       e8 fc 2a e2 ff          callq  405cd0 <malloc@plt>
# 5e31d4:       48 85 c0                test   %rax,%rax

# This shows us that 0x5e31d4 is the relevant code address.

# We can jump directly to stage 4 of the discovery with that and leave the
# heap start and end offsets at 0x0 (NULL) as we already know the unique
# code address and malloc size.
