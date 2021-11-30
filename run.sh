#!/bin/bash  

make clean
make 

   ans1_gpp=$(./cacheSim ./traces/g++.trace)
    ans2_grep=$(./cacheSim ./traces/grep.trace)
    ans3_ls=$(./cacheSim ./traces/ls.trace)
    ans4_plamap=$(./cacheSim ./traces/plamap.trace)
    ans5_testgen=$(./cacheSim ./traces/testgen.trace)
    ans6_tiny=$(./cacheSim ./traces/tiny.trace)

# ans1_gpp=$(./cacheSim ./traces/g++.trace)
# ans2_grep=$(./cacheSim ./traces/grep.trace)
# ans3_ls=$(./cacheSim ./traces/ls.trace)
# ans4_plamap=$(./cacheSim ./traces/plamap.trace)
# ans5_testgen=$(./cacheSim ./traces/testgen.trace)
# ans6_tiny=$(./cacheSim ./traces/tiny.trace)
 
 printf 'OUTPUT FOR THE %s\n' "$1" >>average_amat_output_log.txt
printf 'sum = %f\n' "$(bc <<<"($ans1_gpp+$ans2_grep+$ans3_ls+$ans4_plamap+$ans5_testgen+$ans6_tiny)")" >>average_amat_output_log.txt

