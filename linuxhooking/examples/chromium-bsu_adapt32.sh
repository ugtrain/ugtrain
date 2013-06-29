#!/bin/bash

# The issue is that memory areas for mallocs can differ from distribution to
# distribution. But the thing which remains constant is often the way how
# the code internally works.

# chromium-bsu 0.9.14.1 or 0.9.15

# We already know that chromium-bsu is a 32-bit C++ application. Therefore,
# objects are initialized most likely by the "_Znwj" function. This function
# calls malloc internally.
# And from previous discovery runs we already know the malloc size (272 or
# 0x110) for the object in which the NUMBER OF LIVES is stored.
objdump -xD `which chromium-bsu` | grep "_Znwj" -B 2 -A 1 | grep -A 3 0x110

# Should return something like this:
#  805720d:	c7 04 24 10 01 00 00 	movl   $0x110,(%esp)
#  8057214:	e8 db 36 ff ff       	call   804a8f4 <_Znwj@plt>
#  8057219:	89 c3                	mov    %eax,%ebx
#--

# This shows us that 0x8057219 is the relevant code address.

# We can jump directly to stage 4 of the discovery with that and leave the
# heap start and end addresses at 0x0 (NULL) as we already know the unique
# code address and malloc size.
