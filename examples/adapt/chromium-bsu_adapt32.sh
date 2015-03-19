#!/bin/sh

# Tested with: chromium-bsu 0.9.14.1, 0.9.15, 0.9.15.1

# We already know that chromium-bsu is a 32-bit C++ application. Therefore,
# objects are allocated most likely by the "_Znwj" function. This function
# calls malloc internally.
# And from previous discovery runs we already know the malloc size (272 or
# 0x110) for the HeroAircraft object.

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"
APP_PATHS="\
/usr/games/chromium-bsu
"
RC=0
MSIZE="0x110"

. ./_common_adapt.sh

# check for changed paths
get_app_path "$APP_PATH" "$APP_PATHS"

get_malloc_code "$APP_PATH" "\<_Znwj@plt\>" "$MSIZE," 3 3 3
if [ $RC -ne 0 ]; then
    get_malloc_code "$APP_PATH" "\<_Znwj@plt\>" "$MSIZE," 4 4 4
fi
if [ $RC -ne 0 ]; then exit 1; fi

RESULT=`echo "1;${PATH_RESULT}HeroAircraft;$MSIZE;0x$CODE_ADDR"`
echo "$RESULT"

# Should return something like this:
#  805720d:	c7 04 24 10 01 00 00 	movl   $0x110,(%esp)
#  8057214:	e8 db 36 ff ff       	call   804a8f4 <_Znwj@plt>
#  8057219:	89 c3                	mov    %eax,%ebx

# This shows us that 0x8057219 is the relevant code address.

# We can jump directly to stage 4 of the discovery with that.
