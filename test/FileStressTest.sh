#!/bin/bash
mkdir fileStressTest  
cd fileStressTest

dd if=/dev/urandom of=test.dat bs=1M count=10

num_files=128
stride_size=16

for i in `seq 1 $num_files`; do ln -s `pwd`/test.dat `pwd`/test${i}.dat ; done

../../src/server/server 5111 localhost 2>&1 > server.log &

sleep 10

cat <<EOT > file_tx.sh
# !/bin/bash

start_index=\$1
number_tasks=\$2


echo "si: \$start_index nt \$number_tasks"

for i in \`seq \$start_index \$(( start_index + number_tasks - 1 )) \`; do

echo "localhost:5111:0:0:0:1048576:`pwd`/test\${i}.dat|" > test\${i}.dat.meta.in
LD_PRELOAD=../../src/client/libclient.so ../TazerCp test\${i}.dat.meta.in /dev/null &
done 

wait
rm *meta.in
EOT
chmod +x file_tx.sh


for i in `seq 1 $stride_size $num_files`; do
./file_tx.sh $i $stride_size 2>&1 > /dev/null
echo "num opened $i"
lsof -u frie869 2>&1 | grep "test*.dat" | wc -l
done;

rm test*.dat

../CloseServer localhost 5111



