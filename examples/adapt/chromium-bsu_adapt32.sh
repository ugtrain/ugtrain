#!/bin/bash

# Tested with: chromium-bsu 0.9.14.1, 0.9.15, 0.9.15.1

# We already know that chromium-bsu is a 32-bit C++ application. Therefore,
# objects are allocated most likely by the "_Znwj" function. This function
# calls malloc internally.
# And from previous discovery runs we already know the malloc size (272 or
# 0x110) for the HeroAircraft object.

CWD=`dirname $0`
cd "$CWD"
APP_PATH="$1"

. _common_adapt.sh

get_malloc_code_3 "$APP_PATH" "\<_Znwj@plt\>" "0x110" 3 3

RESULT=`echo "1;HeroAircraft;0x$CODE_ADDR"`
echo "$RESULT"

# Should return something like this:
#  805720d:	c7 04 24 10 01 00 00 	movl   $0x110,(%esp)
#  8057214:	e8 db 36 ff ff       	call   804a8f4 <_Znwj@plt>
#  8057219:	89 c3                	mov    %eax,%ebx

# This shows us that 0x8057219 is the relevant code address.

# We can jump directly to stage 4 of the discovery with that and leave the
# heap start and end offsets at 0x0 (NULL) as we already know the unique
# code address and malloc size.
