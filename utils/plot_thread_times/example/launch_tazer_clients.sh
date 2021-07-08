#!/bin/bash

workspace=$1
data_path=$2
tazer_path=$3
build_dir=$4
server_nodes=$5
server_port=$6

simdir=$1/tazer-bigflow-sim

#clean up previous run
rm -r ${workspace}/client*

test_path=${tazer_path}/utils/plot_thread_times/example/multithreaded_test.sh

echo "$0 $1 $2 $3 $4 $5"

server_addr=$server_nodes #this currently assumes just a single server...

#CACHE_SIZES=(256*1024*1024  2*1024*1024*1024)
CACHE_SIZES=(1024*1024*1024)

#clean up global bounded file cache from previous
global_cache_path=${workspace}/tazer_cache/gc
rm -r ${global_cache_path}
#create localtion of global cache
mkdir -p ${global_cache_path}

node=`hostname`
node_workspace=${workspace}/client/${node}
mkdir -p ${node_workspace}
cd ${node_workspace}

test_id=0
for shared_mem in ${CACHE_SIZES[@]}; do
    for burst_buffer in ${CACHE_SIZES[@]}; do
        for bounded_filelock in ${CACHE_SIZES[@]}; do

            #clean up previous caches
            rm -r /tmp/*tazer${USER}*
            rm -r /state/partition1/*tazer${USER}*
            rm /dev/shm/*tazer${USER}*

            #prepare caches
            mkdir -p /tmp/tazer${USER}/tazer_cache
            #global filelock
            ln -s $global_cache_path /tmp/tazer${USER}/tazer_cache/
            #burst_buffer
            mkdir -p /state/partition1/tazer${USER}/tazer_cache/bbc
            ln -s /state/partition1/tazer${USER}/tazer_cache/bbc  /tmp/tazer${USER}/tazer_cache/

            ####
            #srun exec_tests.sh ${workspace} ${data_path} ${tazer_path} ${build_dir} ${server_addr} ${server_port} ${global_cache_path} ${simdir} $shared_mem $burst_buffer $bounded_filelock $test_id
            srun ${test_path} ${node_workspace} ${data_path} ${tazer_path} ${build_dir} ${server_addr} ${server_port} ${test_id} ${shared_mem} ${burst_buffer} ${bounded_filelock}

            #clean up global bounded file cache from previous
            rm -r ${global_cache_path}
            #create localtion of global cache
            mkdir -p ${global_cache_path}
            test_id=1 #mark not first test
            ####
      done
    done
done

echo "Finished"

exit 0