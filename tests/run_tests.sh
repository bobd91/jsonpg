#!/bin/bash

pcount=0
fcount=0
passed="\e[1;32m"
failed="\e[1;31m"
reset="\e[0m"

failed_msg() {
        echo -e "${failed}${1}${reset}"
}

passed_msg() {
        echo -e "${passed}${1}${reset}"
}

for infile in json/input/*.json; do
        file=$(basename $infile)
        outfile="json/passed/$file"
        if ./jsonpg $infile > temp.json 2>/dev/null; then
                if [ ! -f $outfile ]; then
                        ((++fcount))
                        failed_msg "Unexpected pass: ${file}"
                elif ! diff -w temp.json $outfile > /dev/null; then
                        ((++fcount))
                        failed_msg "Unexpected output: $file"
                else
                        ((++pcount))
                fi
        else
                if [ -f $outfile ]; then
                        ((++fcount))
                        failed_msg "Unexpected fail: $file"
                else
                        ((++pcount))
                fi
                cp $infile json/failed
        fi
done

if [ -f temp.json ]; then
        rm temp.json
fi

if [ $fcount -eq 0 ]; then
        if [ $pcount -eq 1 ]; then
                passed_msg "${pcount} test passed"
        else
                passed_msg "${pcount} tests passed"
        fi
else
        if [ $fcount -eq 1 ]; then
                failed_msg "${fcount} test failed, ${pcount} passed"
        else
                failed_msg "${fcount} tests failed, ${pcount} passed"
        fi
fi

