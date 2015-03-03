#!/bin/sh

# Tested with: sauerbraten 0.0.20100728.dfsg

# We already know that sauerbraten is a 32-bit C++ application. Therefore,
# objects are allocated most likely by the "_Znwj" function. This function
# calls malloc internally.
# And from previous discovery runs we already know the malloc size (1376 or
# 0x560) for the FPSent object (first person shooter entity).

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"
APP_PATH1="/usr/lib/games/sauerbraten/sauer_client"
APP_PATH2="/usr/lib/cube2/cube2_client"
PATH_RESULT=""
RC=0
MSIZE1="0x560"

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
get_malloc_code "$APP_PATH" "\<_Znwj@plt\>" "$MSIZE1," 3 11 7
MSIZE="$MSIZE1"
if [ $RC -ne 0 ]; then exit 1; fi

RESULT=`echo "1;${PATH_RESULT}FPSent;$MSIZE;0x$CODE_ADDR"`
echo "$RESULT"

# Should return something like this:
# 81dc986:	c7 04 24 60 05 00 00 	movl   $0x560,(%esp)
# 81dc98d:	e8 3e ea e6 ff       	call   804b3d0 <_Znwj@plt>
# 81dc992:	c7 00 00 00 00 00    	movl   $0x0,(%eax)

# This shows us that 0x81dc992 is the relevant code address.

# We can jump directly to stage 4 of the discovery with that and leave the
# heap start and end offsets at 0x0 (NULL) as we already know the unique
# code address and malloc size.
