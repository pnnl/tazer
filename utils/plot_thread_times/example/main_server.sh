#!/bin/bash

workspace=$1
tazer_path=$2
build_dir=$3
tazer_server_port=$4


mkdir -p $workspace/server_main
server_dir=$workspace/server_main
cd $server_dir
# echo "$workspace $tazer_path $build_dir"

#rm tazer1GB.dat
touch tazerWrite.dat
if [ ! -f tazer1GB.dat ]; then
    dd if=/dev/urandom of=tazer1GB.dat bs=10M count=100 &
fi
wait

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

TAZER_SERVER=$TAZER_BUILD_DIR/src/server/server

#gdb --ex run --ex bt --args env
TAZER_SERVER_CACHE_SIZE=$((128*1024*1024)) $TAZER_SERVER $SERVER_PORT "$SERVER_ADDR" 2>&1 | tee "server_${SERVER_ADDR}".log
