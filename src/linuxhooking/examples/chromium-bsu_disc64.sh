#!/bin/bash

# Our goal is to find the allocation of the object where the NUMBER OF LIVES
# is stored. This object should be allocated in a constructor at the
# application start.

# We need to run "scanmem" in parallel to search statically for the interesting
# memory address. The game starts with 4 lives (4 little aircrafts at the upper
# right corner). We gradually loose lives and search again in memory to obtain
# a single memory address. This address is matched against the found memory
# areas returned by the malloc calls.

# If we know the memory allocation size, the code address and the offset on
# the stack where the code address is stored, we are ready here. As a
# by-product we also get the offset within the object where the lives are
# stored. This is needed for ugtrain.

# The offset on the stack is relative to the stack end as the stack end isn't
# a static value between multiple application runs.
# The same applies for the heap filter as Linux likely uses heap randomization.

CWD=`dirname $0`
cd "$CWD"
cd ".."
CWD=`pwd`
make
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$CWD/.libs"

mkfifo -m 0666 /tmp/memhack_in > /dev/null 2>&1
mkfifo -m 0666 /tmp/memhack_out > /dev/null 2>&1

# set config for 64-bit chromium-bsu 0.9.15-1.2 (OpenSuse 11.4)
# Format: "1 char stage number (1..4);"
# Stage1: "heap offs min; heap offs max;"  <-- use 0x0 on both to get all mallocs (and frees)
# Stage2: "malloc size;"
# Stage3: "code addr min; code addr max;" (from .text for backtracing)
# Stage4: "[unique code addr of the interesting malloc]" <-- optional
#bash -c 'echo "1;0x0;0x0" > /tmp/memhack_in; cat /tmp/memhack_out > `dirname $0`/memhack_file' &
#bash -c 'echo "2;0x200000;0x350000;288" > /tmp/memhack_in; cat /tmp/memhack_out' &
bash -c 'echo "3;0x200000;0x350000;288;0x404d50;0x425c88" > /tmp/memhack_in; cat /tmp/memhack_out' &
#bash -c 'echo "4;0x200000;0x350000;288;0x404d50;0x425c88;0x411097" > /tmp/memhack_in; cat /tmp/memhack_out' &

# discovered stack offset:     0x198    (after stage 4)
# discovered offset in object: 0xbc     (LIVES)

# call the game here
APP_NAME="chromium-bsu"
./ugpreload ".libs/libmemdisc64.so" `which $APP_NAME` &
pid=$!
echo pid = $pid
wait $pid
