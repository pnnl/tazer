#!/bin/bash

#This sets up, compiles, and runs the Lib.cpp unit test. LibUnitTest.cpp uses the catch2 framework (catch.hpp).
workspace=$1
mkdir -p $workspace/client
cd $workspace/client
tazer_path=$2
build_dir=$3
server_nodes=$4
unit_test_path=$tazer_path/unit_test

SERVER_ADDR=$server_nodes #this currently assumes just a single server...
SERVER_PORT="5001"

module load gcc/8.1.0

#set up data directories and meta files:
mkdir -p ./test_results
mkdir -p local_data
mkdir -p tazer_data

#Get a local copy of the tazer1GB.dat found on the server node.
rm tazer_data/*
ln -s ${workspace}/server/tazer_data/tazer1GB.dat tazer_data/tazer1GB.dat
# scp $SERVER_ADDR:$workspace/tazer_data/tazer1GB.dat $workspace/tazer_data

if [ ! -f local_data/local1GB.dat ]; then
    dd if=/dev/urandom of=local_data/local1GB.dat bs=10M count=100
fi

if [ -f tazer_data/tazer_write.txt ]; then
    rm tazer_data/tazer_write.txt
fi

#Each of the unit tests that require writing to the server get their own meta.in and meta.out file.
compression=0
blocksize=1048576
echo "${SERVER_ADDR}:${SERVER_PORT}:${compression}:0:0:${blocksize}:tazer_data/tazer1GB.dat|" | tee local_data/tazer1GB.dat.meta.in
write_files=(tazerWrite.txt write.txt tazerVector.txt writev.txt tazerFwrite.txt fwrite.txt tazerFputc.txt fputc.txt tazerFputs.txt fputs.txt)
for i in "${write_files[@]}"
do
    echo "${SERVER_ADDR}:${SERVER_PORT}:${compression}:0:0:${blocksize}:tazer_data/$i|" | tee local_data/$i.meta.out
    echo "${SERVER_ADDR}:${SERVER_PORT}:${compression}:0:0:${blocksize}:tazer_data/$i|" | tee local_data/$i.meta.in
done


lib_path=${tazer_path}/${build_dir}/src/client/

#Compiling LibUnitTest.cpp
g++ -I${tazer_path}/inc/ -I${tazer_path}/src/client/ -L${lib_path} -lclient -ldl -o ${unit_test_path}/LibUnitTest ${unit_test_path}/LibUnitTest.cpp

#Run LibUnitTest with the option set to create a junit xml file in test_results.
time TAZER_PREFETCH=0 LD_PRELOAD="${lib_path}libclient.so" TAZER_LOCAL_FILES="local_data/local1GB.dat" ${unit_test_path}/LibUnitTest -o ${workspace}/client/test_results/lib_unit_test_out.txt

#close the server
${tazer_path}/${build_dir}/test/CloseServer $SERVER_ADDR $SERVER_PORT
