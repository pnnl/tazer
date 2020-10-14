#!/bin/bash

workspace=$1
mkdir -p $workspace/server
cd $workspace/server
tazer_path=$2
build_dir=$3
# echo "$workspace $tazer_path $build_dir"

# echo $(hostname) | tee $tazer_path/unit_test/server_hostname.txt
MY_HOSTNAME=`hostname`
ulimit -n 4096

TAZER_BUILD_DIR=$tazer_path/$build_dir


SERVER_DATA_PATH=tazer_data #relative to where the server was executed
#SERVER_ADDR="127.0.0.1"
SERVER_ADDR="$MY_HOSTNAME"
SERVER_PORT="5001"

echo "server host: $MY_HOSTNAME"


if [ ! -f $SERVER_DATA_PATH/tazer100MB.dat ]; then
    mkdir -p $SERVER_DATA_PATH
    dd if=/dev/urandom of=$SERVER_DATA_PATH/tazer100MB.dat bs=10M count=10
fi
#Remove files that were copied from clients to this node in previous tests (tazer_cp_write_test_client.sh).
rm $SERVER_DATA_PATH/client*.dat


TAZER_SERVER_PATH=$TAZER_BUILD_DIR/src/server/server

TAZER_SERVER_CACHE_SIZE=$((128*1024*1024)) $TAZER_SERVER_PATH $SERVER_PORT "$SERVER_ADDR" 2>&1 | tee "server_${SERVER_ADDR}".log


