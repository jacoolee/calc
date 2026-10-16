#!/usr/bin/env bash
# set -x

On_Red='\033[41m'         # Red
Color_Off='\033[0m'       # Text Reset

cat testcase.txt | while read i; do
    if echo "${i}" | grep '=' &>/dev/null; then
        v=$(echo "$i" | cut -d'=' -f1)
        e=$(echo "$i" | cut -d'=' -f2- | cut -d'#' -f1)
        v2=$(./calc "$e")
        if [ ${v2// /} != ${v// /} ]; then
            echo -e "${On_Red}WRONG: $v2 != $i${Color_Off}"
        else
            if [ $# -ge 1 ]; then
                echo "OK: $v2 == $i"
            fi
        fi
    else
        if [ $# -ge 1 ]; then
            echo $i
        fi
    fi
done

