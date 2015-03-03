# The issue is that code address and stack offsets for mallocs can differ
# between distributions, compilers and game versions. But the thing which
# remains constant is often the way how the code internally works.

PFX="[adaption]"
DEBUG=0
if [ -n "$2" ]; then
    if [ "$2" = "DEBUG" ]; then
        DEBUG=1
    fi
fi

CODE=""
OLD_FNAME=""
FUNC_CALLS=""

get_malloc_code()
{
    app_path="$1"
    fname="$2"
    msize="$3"
    reslines=$4
    explines=$5
    taillines=$6
    isunique=0
    RC=0

    alines=`expr $reslines - 1`
    blines=`expr $alines - 1`
    if [ $blines -lt 2 ]; then blines=2; fi

    IFS=`printf '\n+'`
    if [ -z "$CODE" ]; then
        if [ $DEBUG -eq 1 ]; then echo "objdump -D $app_path"; fi
        CODE=`objdump -D "$app_path"`
    fi
    if [ "$OLD_FNAME" != "$fname" ]; then
        if [ $DEBUG -eq 1 ]; then echo "echo \$CODE | grep $fname -B $blines -A 1"; fi
        FUNC_CALLS=`echo "$CODE" | grep "$fname" -B $blines -A 1`
        OLD_FNAME="$fname"
    fi
    if [ $DEBUG -eq 1 ]; then echo "echo \$FUNC_CALLS | grep -A $alines $msize"; fi
    CODE_PART=`echo "$FUNC_CALLS" | grep -A $alines "$msize"`
    if [ -z "$CODE_PART" ]; then RC=1; return; fi
    if [ $DEBUG -eq 1 ]; then echo "$CODE_PART"; fi

    CODE_LINES=`echo "$CODE_PART" | wc -l`
    if [ $CODE_LINES -eq $reslines ]; then
        isunique=1
    elif [ $CODE_LINES -ne $explines ]; then
	RC=1; return
    fi

    if [ $isunique -ne 1 ]; then
        CODE_PART=`echo "$CODE_PART" | tail -n $taillines`
        CODE_PART=`echo "$CODE_PART" | head -n $reslines`

        CODE_LINES=`echo "$CODE_PART" | wc -l`
        if [ $CODE_LINES -ne $reslines ]; then RC=1; return; fi
    fi

    CODE_CALL=`echo "$CODE_PART" | cut -d '
' -f $alines | grep call`
    if [ -z "$CODE_CALL" ]; then RC=1; return; fi
    if [ $DEBUG -eq 1 ]; then echo "CODE_CALL:"; echo "$CODE_CALL"; fi

    CODE_ADDR=`echo "$CODE_PART" | tail -n 1 | cut -d ':' -f 1 | tr -d [:blank:]`
    if [ -z "$CODE_ADDR" ]; then RC=1; return; fi
    if [ $DEBUG -eq 1 ]; then echo "CODE_ADDR:"; echo "$CODE_ADDR"; fi
    IFS=''
}
