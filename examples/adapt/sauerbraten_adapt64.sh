#!/bin/bash

# Tested with: sauerbraten 0.0.20130203.dfsg

# We already know that sauerbraten is a 64-bit C++ application. Therefore,
# objects are allocated most likely by the "_Znwm" function. This function
# calls malloc internally. But here malloc is used directly.
# And from previous discovery runs we already know the malloc size (1456 or
# 0x5b0) for the FPSent object (first person shooter entity).

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"
RC=0

. _common_adapt.sh

get_malloc_code "$APP_PATH" "\<malloc@plt\>" "0x5b0" 7 31 23
if [ $RC -ne 0 ]; then exit 1; fi

RESULT=`echo "1;FPSent;0x$CODE_ADDR"`
echo "$RESULT"

# Should return something like this:
# 81dc986:	c7 04 24 60 05 00 00 	movl   $0x560,(%esp)
# 81dc98d:	e8 3e ea e6 ff       	call   804b3d0 <_Znwj@plt>
# 81dc992:	c7 00 00 00 00 00    	movl   $0x0,(%eax)

# This shows us that 0x81dc992 is the relevant code address.

# We can jump directly to stage 4 of the discovery with that and leave the
# heap start and end offsets at 0x0 (NULL) as we already know the unique
# code address and malloc size.
