#!/bin/sh

# Tested with: sauerbraten 0.0.20100728.dfsg, 0.0.20130203.dfsg

# We already know that sauerbraten is a 64-bit C++ application. Therefore,
# objects are allocated most likely by the "_Znwm" function. This function
# calls malloc internally. But in latest versions malloc is used directly.
# And from previous discovery runs we already know the malloc size for the
# FPSent object (first person shooter entity).

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"
RC=0
MSIZE1="0x598"
MSIZE2="0x5b0"

. ./_common_adapt.sh

# try 0.0.20100728.dfsg
get_malloc_code "$APP_PATH" "\<_Znwm@plt\>" "$MSIZE1," 5 15 11
MSIZE="$MSIZE1"
if [ $RC -ne 0 ]; then
    # try 0.0.20130203.dfsg
    get_malloc_code "$APP_PATH" "\<malloc@plt\>" "$MSIZE2," 7 31 23
    if [ $RC -ne 0 ]; then exit 1; fi
    MSIZE="$MSIZE2"
fi

RESULT=`echo "1;FPSent;$MSIZE;0x$CODE_ADDR"`
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
