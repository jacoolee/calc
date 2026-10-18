#!/usr/bin/env bash
# set -x

Green='\033[0;32m'
On_Red='\033[41m'
Color_Off='\033[0m'

cat testcase.txt | while read i; do
    if echo "${i}" | grep '^#' &>/dev/null; then
        echo $i
    elif echo "${i}" | grep '^ *$' &>/dev/null; then
        echo $i
    else
        v=$(echo "$i" | cut -d'=' -f1)
        e=$(echo "$i" | cut -d'=' -f2- | cut -d'#' -f1)
        v2=$(./calc "$e")
        if [ ${v2// /} != ${v// /} ]; then
            echo -e "${On_Red}WRONG $v2 != $i${Color_Off}"
        else
            echo -e "${Green}OK${Color_Off} $i"
        fi
    fi
done

