# The issue is that code address and stack offset for mallocs can differ
# between distributions, compilers and game versions. But the thing which
# remains constant is often the way how the code works internally.

PFX="[adapt]"
DEBUG=0
if [ -n "$2" ]; then
    if [ "$2" = "DEBUG" ]; then
        DEBUG=1
    fi
fi

PATH_RESULT=""
CODE=""
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

get_func_addr()
{
    app_path="$1"
    func="$2"
    RC=0

    pr_dbg "objdump -R \"$app_path\" | grep \"$func@\"  | cut -d ' ' -f1"
    FUNC_ADDR=`objdump -R "$app_path" | grep "$func@" | cut -d ' ' -f1`
    pr_dbg "FUNC_ADDR: $FUNC_ADDR"
    pr_dbg "echo $FUNC_ADDR | grep -o \"[1-9a-f].*\""
    FUNC_ADDR=`echo $FUNC_ADDR | grep -o "[1-9a-f].*"`
    pr_dbg "FUNC_ADDR: $FUNC_ADDR"

    pr_dbg "objdump -D \"$app_path\" | grep \"# $FUNC_ADDR <\" | cut -d ':' -f1 | grep -o \"[0-9a-f].*\""
    FUNC_ADDR=`objdump -D "$app_path" | grep "# $FUNC_ADDR <" | cut -d ':' -f1 | grep -o "[0-9a-f].*"`
    pr_dbg "FUNC_ADDR: $FUNC_ADDR"
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
        pr_dbg "CODE=\`objdump -D \"$app_path\"\`"
        CODE=`objdump -D "$app_path"`
        pr_dbg "\$CODE contains `wc -l <<< $CODE` lines."
    fi
    pr_dbg "FUNC_LINES=\`grep \"$fname\" -B $blines -A 1 <<< \$CODE\`"
    FUNC_CALLS=`grep "$fname" -B $blines -A 1 <<< $CODE`
    pr_dbg "\$FUNC_CALLS contains `wc -l <<< $FUNC_CALLS` lines."
    pr_dbg "CODE_PART=\`grep -A $alines $msize <<< \$FUNC_CALLS\`"
    CODE_PART=`grep -A $alines "$msize" <<< $FUNC_CALLS`
    pr_dbg "\$CODE_PART contains `wc -l <<< $CODE_PART` lines ($explines expected)."
    if [ -z "$CODE_PART" ]; then RC=1; return; fi
    pr_dbg "$CODE_PART"

    CODE_LINES=`echo "$CODE_PART" | wc -l`
    if [ $CODE_LINES -eq $reslines ]; then
        isunique=1
    elif [ $explines -gt 0 -a $CODE_LINES -ne $explines ]; then
        RC=1; return
    fi

    if [ $isunique -ne 1 ]; then
        if [ $explines -lt 0 ]; then
            explines=`expr $explines \* -1`
            CODE_PART=`echo "$CODE_PART" | head -n $explines`
        fi
        CODE_PART=`echo "$CODE_PART" | tail -n $taillines`
        CODE_PART=`echo "$CODE_PART" | head -n $reslines`

        CODE_LINES=`echo "$CODE_PART" | wc -l`
        if [ $CODE_LINES -ne $reslines ]; then RC=1; return; fi
    fi

    CODE_CALL=`echo "$CODE_PART" | cut -d '
' -f $alines | grep call`
    if [ -z "$CODE_CALL" ]; then RC=1; return; fi
    pr_dbg "CODE_CALL:"; pr_dbg "$CODE_CALL"

    JMPBACK_CODE=`echo "$CODE_PART" | tail -n 1`
    if [ "$JMPBACK_CODE" = "$CODE_CALL" ]; then RC=1; return; fi
    CODE_ADDR=`echo "$JMPBACK_CODE" | cut -d ':' -f 1 | tr -d [:blank:]`
    if [ -z "$CODE_ADDR" ]; then RC=1; return; fi
    pr_dbg "CODE_ADDR:"; pr_dbg "$CODE_ADDR"
    IFS=''
}

# vim: ai ts=4 sw=4 et sts=4 ft=sh
