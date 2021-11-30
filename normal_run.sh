#!/bin/bash  
make clean
make 

./cacheSim ./traces/g++.trace
./cacheSim ./traces/grep.trace
./cacheSim ./traces/ls.trace
./cacheSim ./traces/plamap.trace
./cacheSim ./traces/testgen.trace
./cacheSim ./traces/tiny.trace