#!/bin/bash

# The issue is that code address and stack offsets for mallocs can differ
# between distributions, compilers and game versions. But the thing which
# remains constant is often the way how the code internally works.

DEBUG=0

function get_malloc_code_3()
{
    app_path="$1"
    fname="$2"
    msize="$3"
    explines=$4
    taillines=$5
    isunique=0

    IFS=`printf '\n+'`
    if [ $DEBUG -eq 1 ]; then echo "objdump -D $app_path | grep $fname -B 2 -A 1 | grep -A 2 $msize"; fi
    CODE_PART=`objdump -D "$app_path" | grep "$fname" -B 2 -A 1 \
              | grep -A 2 "$msize"`
    if [ "$CODE_PART" == "" ]; then exit 1; fi
    if [ $DEBUG -eq 1 ]; then echo -e "$CODE_PART"; fi

    CODE_LINES=`echo -e "$CODE_PART" | wc -l`
    if [ $CODE_LINES -eq 3 ]; then
        isunique=1
    elif [ $CODE_LINES -ne $explines ]; then
	exit 1
    fi

    if [ $isunique -ne 1 ]; then
        CODE_PART=`echo -e "$CODE_PART" | tail -n $taillines`
        CODE_PART=`echo -e "$CODE_PART" | head -n 3`

        CODE_LINES=`echo -e "$CODE_PART" | wc -l`
        if [ $CODE_LINES -ne 3 ]; then exit 1; fi
    fi

    CODE_CALL=`echo -e "$CODE_PART" | cut -d '
' -f 2 | grep call`
    if [ "$CODE_CALL" == "" ]; then exit 1; fi

    CODE_ADDR=`echo -e "$CODE_PART" | cut -d '
' -f 3 | cut -d ':' -f 1 | tr -d [:blank:]`
    if [ "$CODE_ADDR" == "" ]; then exit 1; fi
    IFS=''
}

function get_malloc_code_4()
{
    app_path="$1"
    fname="$2"
    msize="$3"
    explines=$4
    taillines=$5
    isunique=0

    IFS=`printf '\n+'`
    if [ $DEBUG -eq 1 ]; then echo "objdump -D $app_path | grep $fname -B 2 -A 1 | grep -A 3 $msize"; fi
    CODE_PART=`objdump -D "$app_path" | grep "$fname" -B 2 -A 1 \
              | grep -A 3 "$msize"`
    if [ "$CODE_PART" == "" ]; then exit 1; fi
    if [ $DEBUG -eq 1 ]; then echo -e "$CODE_PART"; fi

    CODE_LINES=`echo -e "$CODE_PART" | wc -l`
    if [ $CODE_LINES -eq 4 ]; then
        isunique=1
    elif [ $CODE_LINES -ne $explines ]; then
        exit 1
    fi

    if [ $isunique -ne 1 ]; then
        CODE_PART=`echo -e "$CODE_PART" | tail -n $taillines`
        CODE_PART=`echo -e "$CODE_PART" | head -n 4`

        CODE_LINES=`echo -e "$CODE_PART" | wc -l`
        if [ $CODE_LINES -ne 4 ]; then exit 1; fi
    fi

    CODE_CALL=`echo -e "$CODE_PART" | cut -d '
' -f 3 | grep call`
    if [ "$CODE_CALL" == "" ]; then exit 1; fi

    CODE_ADDR=`echo -e "$CODE_PART" | cut -d '
' -f 4 | cut -d ':' -f 1 | tr -d [:blank:]`
    if [ "$CODE_ADDR" == "" ]; then exit 1; fi
}
