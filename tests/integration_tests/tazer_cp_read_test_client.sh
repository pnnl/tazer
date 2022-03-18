#!/bin/bash

#This tests tazerCp by copying a file from a server to the client's LOCAL_DATA_PATH and comparing
#that copied file with the data file in ../SERVER_DATA_PATH. ../SERVER_DATA_PATH from the client's perspective
#is assumed to have a copy of the actual tazer100MB.dat from the server. This file should get copied to the client's node in launch_tazer_client.sh.

workspace=$1 
data_path=$2
tazer_path=$3
build_dir=$4
server_addr=$5
server_port=$6
test_id=$7
#caches $8 $9 ${10}
ref_time=${11}
mkdir -p ${workspace}/test_${test_id}
cd ${workspace}/test_${test_id}


{
    MY_HOSTNAME=`hostname`
    ulimit -n 4096  
    echo "#### EXECUTING TAZER READ TEST ($TEST_ID) on $MY_HOSTNAME"
    echo "$@"

    TAZER_CP_PATH=${tazer_path}/${build_dir}/test/TazerCp
    TAZER_LIB=${tazer_path}/${build_dir}/src/client/libclient.so

    #This checks arguments $8 $9 $10, if they are 0 then the corresponding cache is not used, if they are not 0
    #then they are assumed to be the cache size of the corresponding cache.
    CACHES=(0 0 0)
    CACHE_SIZES=(0 0 0)
    x=0
    for i in $8 $9 ${10}
    do
        if [ ! $i == 0 ]; then
            CACHES[$x]=1
            CACHE_SIZES[$x]=$i
        fi    
        x=$(($x+1))
    done


    SERVER_DATA_PATH=$data_path 
    LOCAL_DATA_PATH=./data #relative to where the client executes
    mkdir -p $LOCAL_DATA_PATH

    echo "### Creating tazer meta files"
    compression=0
    blocksize=1048576
    infile="$SERVER_DATA_PATH/tazer100MB.dat"
    META_FILE=tazer100MB.dat.meta.in

    #echo "${server_addr}:${server_port}:${compression}:0:0:${blocksize}:${infile}|" | tee ${LOCAL_DATA_PATH}/tazer100MB.dat.meta.in
    echo "TAZER0.1" | tee ${LOCAL_DATA_PATH}/${META_FILE}
    echo "type=input" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "[server]" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "file=${infile}" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "host=${server_addr}" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "port=${server_port}" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "compress=false" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "block_size=${blocksize}" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "save_local=false" >> ${LOCAL_DATA_PATH}/${META_FILE}
    echo "prefetch=false" >> ${LOCAL_DATA_PATH}/${META_FILE}
    out_file=./log.out

    echo "TAZER_SHARED_MEM_CACHE=${CACHES[0]} TAZER_SHARED_MEM_CACHE_SIZE=$((${CACHE_SIZES[0]})) \
    TAZER_BB_CACHE=${CACHES[1]} TAZER_BB_CACHE_SIZE=$((${CACHE_SIZES[1]})) \
    TAZER_BOUNDED_FILELOCK_CACHE=${CACHES[2]} TAZER_BOUNDED_FILELOCK_CACHE_SIZE=$((${CACHE_SIZES[2]}))"
    ref_time=0 #debug -- calcuate a reference time (e.g. with SimplePTP if you want to use)
    echo "ref_time ${ref_time}"
    #time gdb --ex run --ex bt --args env 
    time TAZER_REF_TIME=${ref_time} TAZER_SHARED_MEM_CACHE=$((${CACHES[0]})) TAZER_SHARED_MEM_CACHE_SIZE=$((${CACHE_SIZES[0]})) \
    TAZER_BB_CACHE=$((${CACHES[1]})) TAZER_BB_CACHE_SIZE=$((${CACHE_SIZES[1]})) \
    TAZER_BOUNDED_FILELOCK_CACHE=${CACHES[2]} TAZER_BOUNDED_FILELOCK_CACHE_SIZE=$((${CACHE_SIZES[2]})) \
    TAZER_PREFETCH=0 LD_PRELOAD=${TAZER_LIB} ${TAZER_CP_PATH} ${LOCAL_DATA_PATH}/tazer100MB.dat.meta.in ${LOCAL_DATA_PATH}/tazer100MB.dat 
# /share/apps/gcc/8.1.0/lib64/libasan.so:
    if [[ $? -ne 0 ]]; then
        echo "********** Error: Tazer failed **********"
        echo "********** Error: Tazer failed **********" 1>&2
        echo "Details:" 1>&2
        if [ ! "CACHE[0]" == "0" ]; then 
            echo "Shared Memory Enabled: ${CACHE_SIZES[0]}" 1>&2 
        fi
        if [ ! "CACHE[1]" == "0" ]; then 
            echo "Burst Buffer Enabled: ${CACHE_SIZES[1]}" 1>&2
        fi
        if [ ! "CACHE[2]" == "0" ]; then 
            echo "Bounded Filelock Enabled: ${CACHE_SIZES[2]}" 1>&2
        fi  
        exit 1
    fi

    echo "$infile"
    #For the comparison test we assume that ../${SERVER_DATA_PATH}/tazer100MB.dat is a copy of the actual tazer100MB.dat from the server.
    #This file gets copied in launch_tazer_client.sh.
    if [[ $( cmp ${infile} ${LOCAL_DATA_PATH}/tazer100MB.dat) ]]; then
    #Error details are sent to stderr which is captured in client#.error assuming this is run through launch_tazer_client.sh and run_tazer_cp_tests.sh.
        echo "********** Error: comparison failed **********"
        echo "********** Error: comparison failed **********" 1>&2
        echo "Details:" 1>&2
        if [ ! "CACHE[0]" == "0" ]; then 
            echo "Shared Memory Enabled: ${CACHE_SIZES[0]}" 1>&2 
        fi
        if [ ! "CACHE[1]" == "0" ]; then 
            echo "Burst Buffer Enabled: ${CACHE_SIZES[1]}" 1>&2
        fi
        if [ ! "CACHE[2]" == "0" ]; then 
            echo "Bounded Filelock Enabled: ${CACHE_SIZES[2]}" 1>&2
        fi  
        exit 1
    fi
} > test_${test_id}.out 2>test_${test_id}.err

exit 0
