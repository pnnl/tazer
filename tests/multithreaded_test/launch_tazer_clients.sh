#!/bin/bash

workspace=$1
data_path=$2
tazer_path=$3
build_dir=$4
server_nodes=$5
server_port=$6
num_clients=$7
num_nodes=$8

simdir=$1/tazer-bigflow-sim

#clean up previous run
rm -r ${workspace}/client*

integration_test_path=$tazer_path/tests/multithreaded_test

echo "$0 $1 $2 $3 $4 $5"

server_addr=$server_nodes #this currently assumes just a single server...
server_port="5001"

CACHE_SIZES=(256*1024*1024  2*1024*1024*1024)
#CACHE_SIZES=(1024*1024*1024)

#clean up global bounded file cache from previous
global_cache_path=${workspace}/tazer_cache/gc
rm -r ${global_cache_path}
#create localtion of global cache
mkdir -p ${global_cache_path}

test_id=0
for shared_mem in ${CACHE_SIZES[@]}; do
    for burst_buffer in ${CACHE_SIZES[@]}; do
        for bounded_filelock in ${CACHE_SIZES[@]}; do
            srun exec_tests.sh ${workspace} ${data_path} ${tazer_path} ${build_dir} ${server_addr} ${server_port} ${global_cache_path} ${simdir} $shared_mem $burst_buffer $bounded_filelock $test_id

            #clean up global bounded file cache from previous
            rm -r ${global_cache_path}
            #create localtion of global cache
            mkdir -p ${global_cache_path}
            test_id=1 #mark not first test
      done
    done
done



echo "Finished"

exit 0