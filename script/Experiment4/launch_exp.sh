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

H_value=${12}
Hb_value=${13}
MC_value=${14}
Sr_value=${15}
Sth_value=${16}
numCl=${17}

#clean up from previous experiments
rm -r /files/${USER}/tazer_output
rm /dev/shm/*tazer*
rm -r /tmp/*tazer*
rm -r /tmp/${USER}
mkdir -p /tmp/tazer${USER}/tazer_cache

#set up traces for experiment
Num_client=${numCl} # 15 #$(($RANDOM%10+10))
search_dir=/files0/belo700/speedracer/test/to_remove/tazer/script/paper_experiments/exp4
list_files=("$search_dir"/*)
traces=""
for (( i = 0 ; i < ${Num_client} ; i++ )); do
   traces+=${list_files[$i]}
   traces+=" "
done

#set up metafiles
cd ${TAZER_ROOT}/script/Experiment4/Results_${exp_type}_${scalable}_${shared}_${filemem}/

file_names="tazer.dat tazer1.dat tazer2.dat tazer3.dat tazer4.dat tazer5.dat tazer6.dat tazer7.dat tazer8.dat tazer9.dat tazer10.dat tazer11.dat tazer12.dat tazer13.dat tazer14.dat"
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
#export TAZER_UMB_THRESHOLD=50

export TAZER_SHARED_MEM_CACHE=${shared}
export TAZER_SHARED_MEM_CACHE_SIZE=$((shared_size*1024*1024))
export TAZER_BOUNDED_FILELOCK_CACHE=$filemem
mkdir ./FileCache/
export TAZER_BOUNDED_FILELOCK_CACHE_PATH=${TAZER_ROOT}/script/Experiment3/Results_${exp_type}_${scalable}_${shared}_${filemem}/FileCache
export TAZER_BOUNDED_FILELOCK_CACHE_SIZE=$((64*1024*1024))

#using defaults for upper level cache sizes 
#export TAZER_SHARED_MEM_CACHE_SIZE=
#export TAZER_BOUNDED_FILELOCK_CACHE_SIZE=
export TAZER_TRACE_HISTOGRAM=0
TAZER_LIB_PATH=${TAZER_BUILD_DIR}src/client/libclient.so


export TAZER_Hb_VALUE=${Hb_value}
export TAZER_H_VALUE=${H_value}
export TAZER_MC_VALUE=${MC_value}
export TAZER_Sr_VALUE=${Sr_value}
export TAZER_UMB_THRESHOLD=${Sth_value}


#change -i value for each client. Make it unique per client.
#if 6S make it 6X
#Don't make it too high so don't go to 60 since that will make it to long to run
count=0
for trace in $traces; do
    exp_command="env LD_PRELOAD=${TAZER_LIB_PATH} ${WORKLOADSIM_PATH} -f ${trace} -i 10.0 -m .meta.in"
    echo $exp_command
    ${exp_command} > Test_${count}_output_${scalable}_${shared}.txt 2>&1 &
    date +"%T"
    count=$(( count + 1 ))
done

wait
