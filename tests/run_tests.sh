#!/bin/bash

root_dir="json"
input_dir="${root_dir}/input"
passed_dir="${root_dir}/passed"
pretty_dir="${root_dir}/pretty"
failed_dir="${root_dir}/failed"
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

if [ ! -d "$failed_dir" ]; then
        mkdir "$failed_dir"
fi

for infile in ${input_dir}/*.json; do
        file=$(basename $infile)
        for s in {1..28}; do
                outdir=$passed_dir
                for p in 13 14 17 18 21 22 25 26; do
                        if [ $s -eq $p ]; then
                                outdir=$pretty_dir
                                break
                        fi
                done
                outfile="${outdir}/${file}"
#                echo "Parse -s $s $infile and compare with $outfile"
                if ./jsonpg -s $s $infile > temp.json 2>/dev/null; then
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

