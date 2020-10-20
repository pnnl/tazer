#!/bin/bash

workspace=$1
mkdir -p $workspace/server
cd $workspace/server
tazer_path=$2
build_dir=$3
echo "$workspace $tazer_path $build_dir"

# echo $(hostname) | tee $tazer_path/unit_test/server_hostname.txt
MY_HOSTNAME=`hostname`
ulimit -n 4096

TAZER_BUILD_DIR=$tazer_path/$build_dir


SERVER_DATA_PATH=tazer_data #relative to where the server was executed
#SERVER_ADDR="127.0.0.1"
SERVER_ADDR="$MY_HOSTNAME"
SERVER_PORT="5001"

echo "server host: $MY_HOSTNAME"


if [ ! -f $SERVER_DATA_PATH/tazer1GB.dat ]; then
    mkdir -p $SERVER_DATA_PATH
    dd if=/dev/urandom of=$SERVER_DATA_PATH/tazer1GB.dat bs=10M count=100
fi
if [ `ls $SERVER_DATA_PATH | wc -l` -gt 1 ]; then
    rm tazer_data/*.txt
fi

rm /dev/shm/*tazer*${USER}*

TAZER_SERVER_PATH=$TAZER_BUILD_DIR/src/server/server

TAZER_SERVER_CACHE_SIZE=$((128*1024*1024)) $TAZER_SERVER_PATH $SERVER_PORT "$SERVER_ADDR" 2>&1 | tee "server_${SERVER_ADDR}".log
# TAZER_SERVER_CACHE_SIZE=$((128*1024*1024)) $tazer_path/tests/unit_test/server_gdb.sh $TAZER_SERVER_PATH $SERVER_PORT "$SERVER_ADDR" 2>&1 | tee "server_${SERVER_ADDR}".log


