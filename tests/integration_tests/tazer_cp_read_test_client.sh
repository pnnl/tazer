#!/bin/bash

#This tests tazerCp by copying a file from a server to the client's LOCAL_DATA_PATH and comparing
#that copied file with the data file in ../SERVER_DATA_PATH. ../SERVER_DATA_PATH from the client's perspective
#is assumed to have a copy of the actual tazer100MB.dat from the server. This file should get copied to the client's node in launch_tazer_client.sh.

workspace=$1
tazer_path=$2
build_dir=$3
server_addr=$4
echo "$0 $1 $2 $3 $4 $5 $6 $7"
server_port="5001"

MY_HOSTNAME=`hostname`
ulimit -n 4096



TAZER_CP_PATH=${tazer_path}/${build_dir}/test/TazerCp
TAZER_LIB=${tazer_path}/${build_dir}/src/client/libclient.so

#This checks arguments $5 $6 $7, if they are 0 then the corresponding cache is not used, if they are not 0
#then they are assumed to be the cache size of the corresponding cache.
CACHES=(0 0 0)
CACHE_SIZES=(0 0 0)
x=0
for i in $5 $6 $7
do
    if [ ! $i == 0 ]; then
        CACHES[$x]=1
        CACHE_SIZES[$x]=$i
    fi    
    x=$(($x+1))
done


SERVER_DATA_PATH=tazer_data 
LOCAL_DATA_PATH=./data #relative to where the client executes
mkdir -p $LOCAL_DATA_PATH

echo "### Creating tazer meta files"
compression=0
blocksize=1048576
infile="$SERVER_DATA_PATH/tazer100MB.dat"


echo "${server_addr}:${server_port}:${compression}:0:0:${blocksize}:${infile}|" | tee ${LOCAL_DATA_PATH}/tazer100MB.dat.meta.in
out_file=./log.out

echo "TAZER_SHARED_MEM_CACHE=${CACHES[0]} TAZER_SHARED_MEM_CACHE_SIZE=$((${CACHE_SIZES[0]})) \
TAZER_BB_CACHE=${CACHES[1]} TAZER_BB_CACHE_SIZE=$((${CACHE_SIZES[1]})) \
TAZER_BOUNDED_FILELOCK_CACHE=${CACHES[2]} TAZER_BOUNDED_FILELOCK_CACHE_SIZE=$((${CACHE_SIZES[2]}))"

time TAZER_SHARED_MEM_CACHE=${CACHES[0]} TAZER_SHARED_MEM_CACHE_SIZE=$((${CACHE_SIZES[0]})) \
TAZER_BB_CACHE=${CACHES[1]} TAZER_BB_CACHE_SIZE=$((${CACHE_SIZES[1]})) \
TAZER_BOUNDED_FILELOCK_CACHE=${CACHES[2]} TAZER_BOUNDED_FILELOCK_CACHE_SIZE=$((${CACHE_SIZES[2]})) \
TAZER_PREFETCH=0 LD_PRELOAD=${TAZER_LIB} ${TAZER_CP_PATH} ${LOCAL_DATA_PATH}/tazer100MB.dat.meta.in ${LOCAL_DATA_PATH}/tazer100MB.dat 

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

server_file=${tazer_path}/runner-test/integration/server/${SERVER_DATA_PATH}/tazer100MB.dat
echo "$server_file"
#For the comparison test we assume that ../${SERVER_DATA_PATH}/tazer100MB.dat is a copy of the actual tazer100MB.dat from the server.
#This file gets copied in launch_tazer_client.sh.
if [[ $( cmp ${server_file} ${LOCAL_DATA_PATH}/tazer100MB.dat) ]]; then
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

exit 0
