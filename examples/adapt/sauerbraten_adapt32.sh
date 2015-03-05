#!/bin/sh

# Tested with: sauerbraten 0.0.20100728.dfsg, 0.0.20140302-1

# We already know that sauerbraten is a 32-bit C++ application. Therefore,
# objects are allocated most likely by the "_Znwj" function. This function
# calls malloc internally. But in latest versions malloc is used directly.
# And from previous discovery runs we already know the malloc size for the
# FPSent object (first person shooter entity).

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"
APP_PATHS="\
/usr/lib/games/sauerbraten/sauer_client
/usr/lib/cube2/cube2_client
"
RC=0
MSIZE1="0x560"
MSIZE2="0x578"

. ./_common_adapt.sh

# check for changed paths
get_app_path "$APP_PATH" "$APP_PATHS"

# try 0.0.20100728.dfsg
MSIZE="$MSIZE1"
get_malloc_code "$APP_PATH" "\<_Znwj@plt\>" "$MSIZE," 3 11 7
if [ $RC -ne 0 ]; then
    # try 0.0.20140302-1
    MSIZE="$MSIZE2"
    get_malloc_code "$APP_PATH" "\<malloc@plt\>" "$MSIZE," 3 15 11
fi
if [ $RC -ne 0 ]; then exit 1; fi

RESULT=`echo "1;${PATH_RESULT}FPSent;$MSIZE;0x$CODE_ADDR"`
echo "$RESULT"

# Should return something like this:
# 81dc986:	c7 04 24 60 05 00 00 	movl   $0x560,(%esp)
# 81dc98d:	e8 3e ea e6 ff       	call   804b3d0 <_Znwj@plt>
# 81dc992:	c7 00 00 00 00 00    	movl   $0x0,(%eax)

# This shows us that 0x81dc992 is the relevant code address.

# We can jump directly to stage 4 of the discovery with that.
