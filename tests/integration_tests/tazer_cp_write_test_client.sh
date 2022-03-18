#!/bin/bash

#This script tests tazerCp by copying a local file (COPY_FILE) to the server. It doesn't have options for caches like tazer_cp_test_client.sh.

workspace=$1 
data_path=$2
tazer_path=$3
build_dir=$4
server_addr=$5
server_port=$6
test_id=$7
ref_time=$8
mkdir -p ${workspace}/test_${test_id}
cd ${workspace}/test_${test_id}

{
    MY_HOSTNAME=`hostname`
    echo "#### EXECUTING TAZER WRITE TEST ($TEST_ID) on $MY_HOSTNAME"
    echo "$@"
    ulimit -n 4096

    TAZER_CP_PATH=${tazer_path}/${build_dir}/test/TazerCp
    TAZER_LIB=${tazer_path}/${build_dir}/src/client/libclient.so

    LOCAL_DATA_PATH=./data #relative to where the client executes
    SERVER_DATA_PATH=$data_path 
    mkdir -p $LOCAL_DATA_PATH
    #create the file we will write to server
    client_file=test_${test_id}.dat
    ln -s $SERVER_DATA_PATH/tazer100MB.dat $LOCAL_DATA_PATH/$client_file

   
    echo "### Creating tazer meta files"
    META_FILE=${client_file}.meta.out
    compression=0
    blocksize=1048576
    copy_file_path="${SERVER_DATA_PATH}/${client_file}" #this should be the absolute path (not relative to where the server is executing)
    #echo "${server_addr}:${server_port}:${compression}:0:0:${blocksize}:${copy_file_path}|" | tee ${LOCAL_DATA_PATH}/${client_file}.meta.out
    echo "TAZER0.1" | tee ${LOCAL_DATA_PATH}/${META_FILE}
    echo "type=output" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "[server]" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "file=${copy_file_path}" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "host=${server_addr}" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "port=${server_port}" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "compress=false" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "block_size=${blocksize}" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "save_local=false" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "prefetch=false" >> ${LOCAL_DATA_PATH}/${META_FILE}

    ref_time=0 #debug -- calcuate a reference time (e.g. with SimplePTP if you want to use)
    echo "ref_time ${ref_time}"
    time TAZER_REF_TIME=${ref_time} TAZER_PREFETCH=0 LD_PRELOAD=${TAZER_LIB} ${TAZER_CP_PATH} ${LOCAL_DATA_PATH}/${client_file} ${LOCAL_DATA_PATH}/${client_file}.meta.out


    local_size=`wc -c ${LOCAL_DATA_PATH}/${client_file} | awk '{print $1}'`
    remote_size=`wc -c ${copy_file_path} | awk '{print $1}'`
    TIME_LIMIT=180
    start_time=$(date +%s)
    while [ "${local_size}" -ne "${remote_size}" ]; do # because writing to the disk is async from server, wait for it to flush
        sleep 1
        remote_size=`wc -c ${copy_file_path} | awk '{print $1}'`
        cur_time=$(date +%s)
        if [ $(( cur_time - start_time)) -gt ${TIME_LIMIT} ]; then
            echo "time limit, $TIME_LIMIT, reached, exiting"
            echo "$client_file file size was: $remote_size/$local_size"
            exit -1
        fi
    done

    if [[ $(cmp ${copy_file_path} ${LOCAL_DATA_PATH}/${client_file}) ]]; then
    #Error details are sent to stderr which is captured in client#.error assuming this is run through launch_tazer_client.sh and run_tazer_cp_tests.sh.
        echo "********** Error: comparison failed **********"
        echo "********** Error: comparison failed **********" 1>&2
        exit 1
    fi
    rm ${copy_file_path}
} > test_${test_id}.out 2>test_${test_id}.err
exit 0
