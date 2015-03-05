# The issue is that code address and stack offsets for mallocs can differ
# between distributions, compilers and game versions. But the thing which
# remains constant is often the way how the code internally works.

PFX="[adapt]"
DEBUG=0
if [ -n "$2" ]; then
    if [ "$2" = "DEBUG" ]; then
        DEBUG=1
    fi
fi

PATH_RESULT=""
CODE=""
OLD_FNAME=""
FUNC_CALLS=""

pr_dbg()
{
    if [ $DEBUG -eq 1 ]; then
        echo "$1" >&2
    fi
}

pr_err()
{
    echo "$PFX $1" >&2
}

get_app_path()
{
    PATH_RESULT=""
    APP_PATH="$1"
    local app_paths="$2"
    local found=0

    if [ -f "$APP_PATH" ]; then
        return
    fi
    local old_ifs="$IFS"
    IFS=`printf "\n+"`
    for path in $app_paths; do
        if [ -f "$path" ]; then
            APP_PATH="$path"
            found=1
            break
        fi
    done
    IFS="$old_ifs"
    if [ $found -eq 0 ]; then
        pr_err "File $APP_PATH does not exist!"; exit 1
    fi
    local proc_name=`basename "$APP_PATH"`
    PATH_RESULT="proc_name;${proc_name};game_binpath;${APP_PATH};"
}

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
        pr_dbg "objdump -D $app_path"
        CODE=`objdump -D "$app_path"`
    fi
    if [ "$OLD_FNAME" != "$fname" ]; then
        pr_dbg "echo \$CODE | grep $fname -B $blines -A 1"
        FUNC_CALLS=`echo "$CODE" | grep "$fname" -B $blines -A 1`
        OLD_FNAME="$fname"
    fi
    pr_dbg "echo \$FUNC_CALLS | grep -A $alines $msize"
    CODE_PART=`echo "$FUNC_CALLS" | grep -A $alines "$msize"`
    if [ -z "$CODE_PART" ]; then RC=1; return; fi
    pr_dbg "$CODE_PART"

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
    pr_dbg "CODE_CALL:"; pr_dbg "$CODE_CALL"

    CODE_ADDR=`echo "$CODE_PART" | tail -n 1 | cut -d ':' -f 1 | tr -d [:blank:]`
    if [ -z "$CODE_ADDR" ]; then RC=1; return; fi
    pr_dbg "CODE_ADDR:"; pr_dbg "$CODE_ADDR"
    IFS=''
}

# vim: ai ts=4 sw=4 et sts=4 ft=sh
