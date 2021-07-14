#!/bin/bash

workspace=$1
tazer_path=$2
build_dir=$3
tazer_server_port=$4
main_server_addr=$5
main_server_port=$6

rm -r $workspace
mkdir -p $workspace/server_intermediate
server_dir=$workspace/server_intermediate
cd $server_dir
# echo "$workspace $tazer_path $build_dir"
#rm -r $workspace/server_main

# echo $(hostname) | tee $tazer_path/unit_test/server_hostname.txt
MY_HOSTNAME=`hostname`
ulimit -n 4096

TAZER_BUILD_DIR=$tazer_path/$build_dir

#SERVER_ADDR="127.0.0.1"
SERVER_ADDR="$MY_HOSTNAME"
SERVER_PORT="$tazer_server_port"

echo "server host: $MY_HOSTNAME"

rm -r /tmp/*tazer${USER}*
rm -r /state/partition1/*tazer${USER}*
rm /dev/shm/*tazer${USER}*
rm /dev/shm/*tazer*
rm -r /tmp/${USER}/*tazer*
rm -r /files0/${USER}/tazer_server_cache 

TAZER_SERVER=$TAZER_BUILD_DIR/src/server/server

echo "### Creating server tazer meta files"
META_FILE=conns.meta

echo "TAZER0.1" | tee ${server_dir}/${META_FILE}
echo "type=forwarding" >> ${server_dir}/${META_FILE}
echo "[server]" >> ${server_dir}/${META_FILE}
echo "host=${main_server_addr}" >> ${server_dir}/${META_FILE}
echo "port=${main_server_port}" >> ${server_dir}/${META_FILE}

#mkdir -p /tmp/*tazer${USER}*/tazer_server_cache


export TAZER_NETWORK_CACHE=1
export TAZER_SERVER_CONNECTIONS=${server_dir}/${META_FILE}
export TAZER_BOUNDED_FILELOCK_CACHE=1
export TAZER_BOUNDED_FILELOCK_CACHE_SIZE=$(( 50*1024*1024*1024 ))
export TAZER_BOUNDED_FILELOCK_CACHE_PATH=/files0/${USER}/tazer_server_cache


#gdb --ex run --ex bt --args env
TAZER_PRIVATE_MEM_CACHE_SIZE=$((128*1024*1024)) TAZER_SERVER_CACHE_SIZE=$((128*1024*1024)) $TAZER_SERVER $SERVER_PORT "$SERVER_ADDR" 2>&1 | tee "server_${SERVER_ADDR}".log

