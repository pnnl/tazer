#!/bin/bash

exp_type=$1
tazer_server=$2
TAZER_ROOT=$3
TAZER_BUILD_DIR=$4
WORKLOADSIM_PATH=$5
scalable=$6
shared=$7
filemem=$8
private_size=$9
shared_size=${10}
block_size=${11}

#clean up from previous experiments
rm -r /files/${USER}/tazer_output
rm /dev/shm/*tazer*
rm -r /tmp/*tazer*
rm -r /tmp/${USER}
mkdir -p /tmp/tazer${USER}/tazer_cache

#set up traces for experiment
if [ "${exp_type}" = "A" ]; then 
    traces="Test1A Test1B"
elif [ "${exp_type}" = "B" ]; then
    traces="Test2A_2S Test2A_4S Test2A_6S"
elif [ "${exp_type}" = "C" ]; then 
    traces="Test2B_2S Test2B_4S Test2B_6S"
elif [ "${exp_type}" = "D" ]; then 
    traces="Test1A Test1B Test2B_2S Test2B_4S Test2B_6S"
elif [ "${exp_type}" = "E" ]; then 
    traces="Test1A Test1B Test2A_2S Test2A_4S Test2A_6S Test2B_2S Test2B_4S Test2B_6S"
else
    echo "Unknown value for experiment: ${exp_type}"
    exit 1
fi

#set up metafiles
cd ${TAZER_ROOT}/script/Experiment3/FriResults_${exp_type}_${scalable}_${shared}_${filemem}/

file_names="tazer.dat tazer2.dat tazer3.dat tazer4.dat tazer5.dat tazer6.dat tazer7.dat tazer8.dat"
for file in $file_names; do
    echo "TAZER0.1" > ${file}.meta.in
    echo "type=input" >> ${file}.meta.in
    echo "[server]" >> ${file}.meta.in
    echo "host=${tazer_server}" >> ${file}.meta.in
    echo "port=6024" >> ${file}.meta.in
    echo "file=${TAZER_ROOT}/script/${file}" >> ${file}.meta.in
done

#set up env variables for TAZER

export TAZER_BLOCKSIZE=$((block_size*1024))

export TAZER_SCALABLE_CACHE=$scalable
export TAZER_PRIVATE_MEM_CACHE_SIZE=$((private_size*1024*1024))
export TAZER_SCALABLE_CACHE_NUM_BLOCKS=$((TAZER_PRIVATE_MEM_CACHE_SIZE/TAZER_BLOCKSIZE))

export TAZER_SHARED_MEM_CACHE=${shared}
export TAZER_SHARED_MEM_CACHE_SIZE=$((shared_size*1024*1024))
export TAZER_BOUNDED_FILELOCK_CACHE=$filemem
mkdir ./FileCache/
export TAZER_BOUNDED_FILELOCK_CACHE_PATH=${TAZER_ROOT}/script/Experiment3/FriResults_${exp_type}_${scalable}_${shared}_${filemem}/FileCache
export TAZER_BOUNDED_FILELOCK_CACHE_SIZE=$((64*1024*1024))

#using defaults for upper level cache sizes 
#export TAZER_SHARED_MEM_CACHE_SIZE=
#export TAZER_BOUNDED_FILELOCK_CACHE_SIZE=
export TAZER_TRACE_HISTOGRAM=0
TAZER_LIB_PATH=${TAZER_BUILD_DIR}src/client/libclient.so


for trace in $traces; do
    exp_command="env LD_PRELOAD=${TAZER_LIB_PATH} ${WORKLOADSIM_PATH} -f ${TAZER_ROOT}/script/paper_experiments/${trace}.txt -i 10.0 -m .meta.in"
    echo $exp_command
    ${exp_command} > ${trace}_output_${scalable}_${shared}.txt 2>&1 &
    date +"%T"
    
done

wait
