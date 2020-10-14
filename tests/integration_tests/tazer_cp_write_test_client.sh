#!/bin/bash

#This script tests tazerCp by copying a local file (COPY_FILE) to the server. It doesn't have options for caches like tazer_cp_test_client.sh.

workspace=$1
tazer_path=$2
build_dir=$3
server_addr=$4
client_name=$5
echo "$0 $1 $2 $3 $4 $5"
server_port="5001"

MY_HOSTNAME=`hostname`
ulimit -n 4096

TAZER_CP_PATH=${tazer_path}/${build_dir}/test/TazerCp
TAZER_LIB=${tazer_path}/${build_dir}/src/client/libclient.so

LOCAL_DATA_PATH=./data #relative to where the client executes
SERVER_DATA_PATH=tazer_data 

client_file=${client_name}.dat

if [ ! -f ${LOCAL_DATA_PATH}/${client_file} ]; then
    mkdir -p $LOCAL_DATA_PATH
    dd if=/dev/urandom of=${LOCAL_DATA_PATH}/${client_file} bs=10M count=10
fi

echo "### Creating tazer meta files"
compression=0
blocksize=1048576
copy_file_path="${SERVER_DATA_PATH}/${client_file}"
echo "${server_addr}:${server_port}:${compression}:0:0:${blocksize}:${copy_file_path}|" | tee ${LOCAL_DATA_PATH}/${client_file}.meta.out
out_file=./log.out
time TAZER_PREFETCH=0 LD_PRELOAD=${TAZER_LIB} ${TAZER_CP_PATH} ${LOCAL_DATA_PATH}/${client_file} ${LOCAL_DATA_PATH}/${client_file}.meta.out


server_file=${tazer_path}/runner-test/integration/server/${copy_file_path}

local_size=`wc -c ${LOCAL_DATA_PATH}/${client_file} | awk '{print $1}'`
remote_size=`wc -c ${server_file} | awk '{print $1}'`
TIME_LIMIT=180
start_time=$(date +%s)
while [ "${local_size}" -ne "${remote_size}" ]; do
    sleep 1
    remote_size=`wc -c ${server_file} | awk '{print $1}'`
    cur_time=$(date +%s)
    if [ $(( cur_time - start_time)) -gt ${TIME_LIMIT} ]; then
        echo "time limit, $TIME_LIMIT, reached, exiting"
        echo "$client_file file size was: $remote_size/$local_size"
        exit -1
    fi
done

if [[ $(cmp ${server_file} ${LOCAL_DATA_PATH}/${client_file}) ]]; then
#Error details are sent to stderr which is captured in client#.error assuming this is run through launch_tazer_client.sh and run_tazer_cp_tests.sh.
    echo "********** Error: comparison failed **********"
    echo "********** Error: comparison failed **********" 1>&2
    exit 1
fi
rm ${server_file}
exit 0
