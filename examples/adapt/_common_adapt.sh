#!/bin/bash

# The issue is that code address and stack offsets for mallocs can differ
# between distributions, compilers and game versions. But the thing which
# remains constant is often the way how the code internally works.

DEBUG=0

CODE=""
OLD_FNAME=""
FUNC_CALLS=""

function get_malloc_code()
{
    app_path="$1"
    fname="$2"
    msize="$3"
    reslines=$4
    explines=$5
    taillines=$6
    isunique=0
    RC=0

    let "alines = $reslines - 1"

    IFS=`printf '\n+'`
    if [ "$CODE" == "" ]; then
        if [ $DEBUG -eq 1 ]; then echo "objdump -D $app_path"; fi
        CODE=`objdump -D "$app_path"`
    fi
    if [ "$OLD_FNAME" != "$fname" ]; then
        if [ $DEBUG -eq 1 ]; then echo "echo -e \$CODE | grep $fname -B 2 -A 1"; fi
        FUNC_CALLS=`echo -e "$CODE" | grep "$fname" -B 2 -A 1`
        OLD_FNAME="$fname"
    fi
    if [ $DEBUG -eq 1 ]; then echo "echo -e \$FUNC_CALLS | grep -A $alines $msize"; fi
    CODE_PART=`echo -e "$FUNC_CALLS" | grep -A $alines "$msize"`
    if [ "$CODE_PART" == "" ]; then RC=1; return; fi
    if [ $DEBUG -eq 1 ]; then echo -e "$CODE_PART"; fi

    CODE_LINES=`echo -e "$CODE_PART" | wc -l`
    if [ $CODE_LINES -eq $reslines ]; then
        isunique=1
    elif [ $CODE_LINES -ne $explines ]; then
	RC=1; return
    fi

    if [ $isunique -ne 1 ]; then
        CODE_PART=`echo -e "$CODE_PART" | tail -n $taillines`
        CODE_PART=`echo -e "$CODE_PART" | head -n $reslines`

        CODE_LINES=`echo -e "$CODE_PART" | wc -l`
        if [ $CODE_LINES -ne $reslines ]; then RC=1; return; fi
    fi

    CODE_CALL=`echo -e "$CODE_PART" | cut -d '
' -f $alines | grep call`
    if [ "$CODE_CALL" == "" ]; then RC=1; return; fi

    CODE_ADDR=`echo -e "$CODE_PART" | cut -d '
' -f $reslines | cut -d ':' -f 1 | tr -d [:blank:]`
    if [ "$CODE_ADDR" == "" ]; then RC=1; return; fi
    IFS=''
}
