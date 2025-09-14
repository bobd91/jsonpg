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

declare -a bar_chars
bar_chars=("\u2588" "\u2589" "\u258A" "\u258B" "\u258C" "\u258D" "\u258E" "\u258F")
complete=0

render_progress() {
        local current=$1
        local len=$2
        local colour=$3
        local percent=$(($current * 100 / $len))
        local suffix="${current}/${len} (${percent}%%)"
        local suffix_max=$((8 + 2 * ${#len}))
        local length=$((COLUMNS - $suffix_max - 1))


        local total_bar=$(($length * 7 * $current / $len))
        local comp_bar=$(($length * 7 * $complete / $len))
        local total_full=$(($total_bar / 7))
        local comp_full=$((comp_bar / 7))
        local rem=$(($total_bar % 7))
        local s=$colour
        local i

        for ((i = $comp_full ; i < $total_full ; i++))
        do
               s+=${bar_chars[1]}
        done
        if [ $rem -gt 0 ]; then
                s+=${bar_chars[8 - $rem]}
        else
                s+=' '
        fi
        for ((i = $total_full + 1 ; i < $length ; i++))
        do
                s+=' '
        done
        s+=$reset
        s+=$(printf "%${suffix_max}s" "${suffix}")
	printf '\e7' # save the cursor location
        printf '\e[%d;%dH' "$LINES" "$comp_full" # move cursor to the bottom line
	  #printf '\e[0K' # clear the line
	printf "$s" # print the progress bar
	printf '\e8' # restore the cursor location
        complete=$current
}

init-term() {
	printf '\n' # ensure we have space for the scrollbar
	printf '\e7' # save the cursor location
	printf '\e[%d;%dr' 0 "$((LINES - 1))" # set the scrollable region (margin)
	printf '\e8' # restore the cursor location
	printf '\e[1A' # move cursor up
        printf '\e[?25l'
}

deinit-term() {
	printf '\e7' # save the cursor location
	printf '\e[%d;%dr' 0 "$LINES" # reset the scrollable region (margin)
	printf '\e[%d;%dH' "$LINES" 0 # move cursor to the bottom line
	printf '\e[0K' # clear the line
	printf '\e8' # reset the cursor location
        printf '\e[?25h'
}

failed_msg() {
        echo -e "${failed}${1}${reset}"
}

passed_msg() {
        echo -e "${passed}${1}${reset}"
}

if [ ! -d "$failed_dir" ]; then
        mkdir "$failed_dir"
fi

main() {
        # ensure winsize gets updated
        (:)
	trap deinit-term exit
	trap init-term winch
	init-term

        local files=(${input_dir}/*.json)
        local len=$((10 * ${#files[@]}))
        local i
        local infile
        for infile in ${files[@]}; do
                local file=$(basename $infile)
                local s
                for s in {1..10}; do
                        local outdir=$passed_dir
                        local p
                        for p in 7 8; do
                                if [ $s -eq $p ]; then
                                        outdir=$pretty_dir
                                        break
                                fi
                        done
                        local outfile
                        outfile="${outdir}/${file}"
                        #echo "Parse -s $s $infile and compare with $outfile"
                        if ./jsonpgtest -s $s $infile > temp.json 2>/dev/null; then
                                if [ ! -f $outfile ]; then
                                        ((++fcount))
                                        failed_msg "Unexpected pass: ${file}"
                                elif ! diff -w temp.json $outfile > /dev/null; then
                                        ((++fcount))
                                        failed_msg "Unexpected output: $file"
                                else
                                        ((++pcount))
                                fi
                        elif [ $? -eq 1 ];then

                                if [ -f $outfile ]; then
                                        ((++fcount))
                                        failed_msg "Unexpected fail: $file"
                                else
                                        ((++pcount))
                                fi
                                cp $infile json/failed
                        else
                                echo "Parse -s $s $infile failed"
                        fi
                        ((i++))
                        if [ $fcount -eq 0 ]; then
                                render_progress "$i" "$len" "$passed"
                        else
                                render_progress "$i" "$len" "$failed"
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
}

main
