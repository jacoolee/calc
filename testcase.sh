#!/usr/bin/env bash

# set -x

Green='\033[0;32m'
On_Red='\033[41m'
Color_Off='\033[0m'

n=0
cat testcase.txt | while read i; do
    n=$((n+1))

    if [ ${1:-0} != 0 ] && [ ${1:-0} -ne ${n} ]; then
        continue
    fi

    if echo "${i}" | grep '^#' &>/dev/null; then
        echo $i
    elif echo "${i}" | grep '^ *$' &>/dev/null; then
        echo $i
    else
        v=$(echo "$i" | cut -d'=' -f1)
        e=$(echo "$i" | cut -d'=' -f2- | cut -d'#' -f1)
        v2=$(./calc "$e" ${2} 2>/tmp/testcase.${n}.err)
        rc=$?
        if [ $rc -eq 0 ]; then
            if [ ${v2// /} != ${v// /} ]; then
                echo -e "${On_Red}NOT EQ${Color_Off} $v2 != $i"
            else
                echo -e "${Green}PASS${Color_Off} $i"
            fi

            cat /tmp/testcase.${n}.err

        else
            echo -e "${On_Red}RC $rc${Color_Off} $i"
        fi
    fi
done

