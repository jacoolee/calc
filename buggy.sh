#!/usr/bin/env bash
# set -x

Green='\033[0;32m'
On_Red='\033[41m'
Color_Off='\033[0m'

n=0
cat buggy.txt | while read i; do
    n=$((n+1))

    if [ ${1:-0} != 0 ] && [ ${1:-0} -ne ${n} ]; then
        continue
    fi

    if echo "${i}" | grep '^#' &>/dev/null; then
        echo $i
    elif echo "${i}" | grep '^ *$' &>/dev/null; then
        echo $i
    else
        e=$(echo "$i" | cut -d'#' -f1)
        x=$(./calc "$e" $2 2> /tmp/buggy.${n}.err)
        if [ $? -eq 0 ]; then
            echo -e "${Green}PASS:$n${Color_Off} $i"
        else
            echo -e "${On_Red}BUGGY:$n${Color_Off} $i"
        fi

        if [ ${2:-0} != 0 ]; then
            cat /tmp/buggy.${n}.err
        fi

        printf '%s' "$x"
        echo
    fi
done

