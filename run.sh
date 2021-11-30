#!/bin/bash

make clean
make
 for i in 1 2 3 4 5 6 7 8 9 10
 do
    ans1_gpp=$(./cacheSim ./traces/g++.trace)
    ans2_grep=$(./cacheSim ./traces/grep.trace)
    ans3_ls=$(./cacheSim ./traces/ls.trace)
    ans4_plamap=$(./cacheSim ./traces/plamap.trace)
    ans5_testgen=$(./cacheSim ./traces/testgen.trace)
    ans6_tiny=$(./cacheSim ./traces/tiny.trace)

     echo "gpp: $ans1_gpp; $ans2_grep; $ans3_ls; $ans4_plamap; $ans5_testgen; $ans6_tiny"
     echo ""
 done

# printf 'OUTPUT FOR THE %s\n' "$1" >>average_amat_output_log.txt
# printf 'sum = %f\n' "$(bc <<<"($ans1_gpp+$ans2_grep+$ans3_ls+$ans4_plamap+$ans5_testgen+$ans6_tiny)")" >>average_amat_output_log.txt
