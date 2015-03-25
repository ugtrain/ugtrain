#!/bin/sh

# Tested with: supertux2 0.3.4-1build1

# We already know that supertux2 is a 64-bit C++ application. Therefore,
# objects are allocated most likely by the "_Znwm" function. This function
# calls malloc internally.
# And from previous discovery runs we already know the malloc size (40 or
# 0x28) for the PlayerStatus object.

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"
APP_PATHS="\
/usr/games/supertux2
"
RC=0
# The size is small. So adaption is very difficult here. But it should be
# the very first _Znwm call with this size.
MSIZE="0x28"

. ./_common_adapt.sh

# check for changed paths
get_app_path "$APP_PATH" "$APP_PATHS"

get_malloc_code "$APP_PATH" "\<_Znwm@plt\>" "$MSIZE," 3 -3 3
if [ $RC -ne 0 ]; then exit 1; fi

RESULT=`echo "1;${PATH_RESULT}PlayerStatus;$MSIZE;0x$CODE_ADDR"`
echo "$RESULT"

# Should return something like this:
#  48ce70:       bf 28 00 00 00          mov    $0x28,%edi
#  48ce75:       e8 d6 75 fd ff          callq  464450 <_Znwm@plt>
#  48ce7a:       48 89 c7                mov    %rax,%rdi

# This shows us that 0x48ce7a is the relevant code address.

# We can jump directly to stage 4 of the discovery with that.
