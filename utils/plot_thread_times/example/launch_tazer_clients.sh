#!/bin/bash

workspace=$1
data_path=$2
tazer_path=$3
build_dir=$4
server_nodes=$5
server_port=$6

ref_time=`date +%s%N`

#clean up previous run
rm -r ${workspace}/client*

test_path=${tazer_path}/utils/plot_thread_times/example/multithreaded_test.sh

echo "$0 $1 $2 $3 $4 $5 $6" 

server_addr=$server_nodes #this currently assumes just a single server...

shared_mem=1024*1024*1024
burst_buffer=1024*1024*1024
bounded_filelock=0

#clean up global bounded file cache from previous
global_cache_path=${workspace}/tazer_cache/gc
#rm -r ${global_cache_path}
rm -r ${workspace}/tazer_cache
#create localtion of global cache
mkdir -p ${global_cache_path}

node=`hostname`
node_workspace=${workspace}/client/${node}
mkdir -p ${node_workspace}
cd ${node_workspace}

test_id=0

#clean up previous caches
rm -r /tmp/*tazer${USER}*
rm -r /state/partition1/*tazer${USER}*
rm /dev/shm/*tazer${USER}*
#rm /dev/shm/*tazer*
rm -r /tmp/${USER}/*tazer*

#prepare caches
mkdir -p /tmp/tazer${USER}/tazer_cache
#global filelock
ln -s $global_cache_path /tmp/tazer${USER}/tazer_cache/
#burst_buffer
mkdir -p /state/partition1/tazer${USER}/tazer_cache/bbc
ln -s /state/partition1/tazer${USER}/tazer_cache/bbc  /tmp/tazer${USER}/tazer_cache/

srun ${test_path} ${node_workspace} ${data_path} ${tazer_path} ${build_dir} ${server_addr} ${server_port} ${test_id} ${shared_mem} ${burst_buffer} ${bounded_filelock}

#clean up global bounded file cache from previous
rm -r ${global_cache_path}
#create localtion of global cache
mkdir -p ${global_cache_path}
test_id=1 #mark not first test

echo "Finished"

exit 0