#!/bin/bash

# Now, we have everything we need to hook on a single malloc call responsible
# for object initialization where the NUMBER OF LIVES is stored.

# ugtrain needs this information to know if the interesting memory area is
# already available or is already freed again. The other thing needed is
# the data offset of the number of lives within the object. This is a
# by-product of the discovery.

CWD=`dirname $0`
cd "$CWD"
cd ".."
CWD=`pwd`
make
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$CWD/.libs"

mkfifo -m 0666 /tmp/memhack_in > /dev/null 2>&1
mkfifo -m 0666 /tmp/memhack_out > /dev/null 2>&1

# set config for 64-bit chromium-bsu 0.9.15-1.2 (OpenSuse 11.4)
# Format: "number of values;"
#         "malloc size; code addr; stack offset of code addr;"
#         "malloc size; ..."
bash -c 'echo "1;288;0x411097;0x198" > /tmp/memhack_in; cat /tmp/memhack_out' &

# call the game here
APP_NAME="chromium-bsu"
./ugpreload ".libs/libmemhack64.so" `which $APP_NAME` &
pid=$!
echo pid = $pid
wait $pid
