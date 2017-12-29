#!/bin/sh

# Tested with: chromium-bsu 0.9.14.1, 0.9.15, 0.9.15.1

# We already know that chromium-bsu is a 64-bit C++ application. Therefore,
# objects are allocated most likely by the "_Znwm" function. This function
# calls malloc internally.
# And from previous discovery runs we already know the malloc size (288 or
# 0x120) for the HeroAircraft object.

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"
APP_PATHS="\
/usr/bin/chromium-bsu
/usr/games/chromium-bsu
"
RC=0
MSIZE="0x120"

. ./_common_adapt.sh

# check for changed paths
get_app_path "$APP_PATH" "$APP_PATHS"

get_malloc_code "$APP_PATH" "\<_Znwm@plt\>" "$MSIZE," 4 4 4
if [ $RC -ne 0 ]; then exit 1; fi

RESULT=`echo "1;${PATH_RESULT}HeroAircraft;$MSIZE;0x$CODE_ADDR"`
echo "$RESULT"

# Should return something like this:
#  411086:       bf 20 01 00 00          mov    $0x120,%edi
#  41108b:       48 89 1d fe 14 24 00    mov    %rbx,0x2414fe(%rip)        # 652590 <_ZTIPc+0x2f0>
#  411092:       e8 31 36 ff ff          callq  4046c8 <_Znwm@plt>
#  411097:       48 89 c7                mov    %rax,%rdi

# This shows us that 0x411097 is the relevant code address.
