#!/usr/bin/env bash

# set -x

Green='\033[0;32m'
On_Red='\033[41m'
Color_Off='\033[0m'

ntotal=0
npass=0
notpass=0
nerror=0

n=0

while read -r l; do
    n=$((n+1))

    if [ ${1:-0} != 0 ] && [ ${1:-0} -ne ${n} ]; then
        continue
    fi

    if echo "${l}" | grep '^#' &>/dev/null; then
        echo $l
    elif echo "${l}" | grep '^ *$' &>/dev/null; then
        echo $l
    else
        ntotal=$((ntotal+1))

        v=$(echo "$l" | cut -d'=' -f1)
        e=$(echo "$l" | cut -d'=' -f2- | cut -d'#' -f1)
        v2=$(./calc "$e" ${2} 2>/tmp/testcase.${n}.err)
        rc=$?

        if [ $rc -eq 0 ]; then
            if [ ${v2// /} != ${v// /} ]; then
                notpass=$((notpass+1))
                echo -e "${On_Red}NOT EQ:$n${Color_Off} $v2 != $l"
                cat /tmp/testcase.${n}.err
                echo
                echo "$e"

                if [ ${ENV_BREAK:-0} -eq 1 ]; then
                    break
                fi

            else
                npass=$((npass+1))
                echo -e "${Green}PASS:$n${Color_Off} $l"
            fi

        else
            nerror=$((nerror+1))
            echo -e "${On_Red}RC $rc:$n${Color_Off} $l"
            cat /tmp/testcase.${n}.err
            echo
            echo "$e"

            if [ ${ENV_BREAK:-0} -eq 1 ]; then
                break
            fi

        fi
    fi
done < testcase.txt

echo
echo "${ntotal} total, ${npass} pass, ${notpass} not pass, ${nerror} error"
