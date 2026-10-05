#!/usr/bin/env bash
# set -x

grep '=' testcase.txt | while read i; do
    v=$(echo "$i" | cut -d'=' -f1)
    e=$(echo "$i" | cut -d'=' -f2- | cut -d'#' -f1)
    v2=$(./calc "$e")
    # echo "$v2" "$v" "$e"
    if [ ${v2// /} != ${v// /} ]; then
        echo "WRONG: calc: $v2, raw: $i"
    fi
done

