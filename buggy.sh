#!/usr/bin/env bash
# set -x

Green='\033[0;32m'
On_Red='\033[41m'
Color_Off='\033[0m'

cat buggy.txt | while read i; do
    if echo "${i}" | grep '^#' &>/dev/null; then
        echo $i
    elif echo "${i}" | grep '^ *$' &>/dev/null; then
        echo $i
    else
        echo -e "${Green}$i${Color_Off}"
        ./calc "$i"
        echo
    fi
done

