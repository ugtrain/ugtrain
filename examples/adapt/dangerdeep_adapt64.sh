#!/bin/bash

# Tested with: dangerdeep 0.4.0

# We already know that dangerdeep is a 64-bit C++ application. Therefore,
# objects are allocated most likely by the "_Znwm" function. This function
# calls malloc internally.
# And from previous discovery runs we already know the malloc size (1784 or
# 0x6f8) for the Submarine object.

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"
APP_PATHS="\
/usr/bin/dangerdeep
/usr/games/dangerdeep
"
RC=0
MSIZE="0x6f8"

. ./_common_adapt.sh

# check for changed paths
get_app_path "$APP_PATH" "$APP_PATHS"

get_malloc_code "$APP_PATH" "\<_Znwm@plt\>" "$MSIZE," 5 17 17
if [ $RC -ne 0 ]; then exit 1; fi

RESULT=`echo "1;${PATH_RESULT}Submarine;$MSIZE;0x$CODE_ADDR"`
echo "$RESULT"
