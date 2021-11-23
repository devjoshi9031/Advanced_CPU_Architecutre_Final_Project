make clean
make 
echo "Running g++ trace!!!"
./CacheSim ./traces/g++.trace >> g++.txt
echo "Running grep trace!!!"
./CacheSim ./traces/grep.trace >> grep.txt
echo "Running ls trace!!!"
./CacheSim ./traces/ls.trace >> ls.txt
echo "Running Plamap trace!!!"
./CacheSim ./traces/plamap.trace >> plamap.txt
echo "Running testgen trace!!!"
./CacheSim ./traces/testgen.trace >> testgen.txt
echo "Running tiny trace!!!"
./CacheSim ./traces/tiny.trace >> tiny.txt