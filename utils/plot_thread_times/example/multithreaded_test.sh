#!/bin/bash
NUM_THREADS=10
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
    SLOT=7
    MYID=117
    MY_HOSTNAME=`hostname`
    ulimit -n 4096  
    echo "#### EXECUTING TAZER MULTITHREADED TEST ($TEST_ID) on $MY_HOSTNAME"
    echo "$@"

    MULTITHREADED_TEST_PATH=${tazer_path}/utils/plot_thread_times/example/multithreaded_test
    TAZER_LIB=${tazer_path}/${build_dir}/src/client/libclient.so
    output_path=${tazer_path}/utils/plot_thread_times/example

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

    N=1
    expName="tazer_4"
    taskType="tazer"
    ioType="tazer"

    t=$(date +%s)
    var_names="StartTime" && var_vals="${t}" && var_times="${t}"
    var_names="${var_names},N" && var_vals="${var_vals},${N}" && var_times="${var_times},${t}"
    var_names="${var_names},ExpName" && var_vals="${var_vals},${expName}" && var_times="${var_times},${t}"
    var_names="${var_names},TaskType" && var_vals="${var_vals},${taskType}" && var_times="${var_times},${t}"
    var_names="${var_names},Host" && var_vals="${var_vals},${MY_HOSTNAME}" && var_times="${var_times},${t}"
    var_names="${var_names},IOType" && var_vals="${var_vals},${ioType}" && var_times="${var_times},${t}"
    var_names="${var_names},Slot" && var_vals="${var_vals},${SLOT}" && var_times="${var_times},${t}"


    SERVER_DATA_PATH=$data_path 
    LOCAL_DATA_PATH=./data #relative to where the client executes
    mkdir -p $LOCAL_DATA_PATH


    t=$(date +%s)
    var_names="${var_names},StartSetup" && var_vals="${var_vals},${t}" && var_times="${var_times},${t}"
    fnum=$(( ${MYID} / 4 ))

    echo "### Creating tazer meta files"
    compression=0
    blocksize=1048576
    infile="$SERVER_DATA_PATH/tazer1GB.dat"
    writefile="$SERVER_DATA_PATH/tazerWrite.dat"
    IN_META_FILE=tazer1GB.meta
    OUT_META_FILE=tazerWrite.meta

    #echo "${server_addr}:${server_port}:${compression}:0:0:${blocksize}:${infile}|" | tee ${LOCAL_DATA_PATH}/tazer1GB.dat.meta.in
    echo "TAZER0.1" | tee ${LOCAL_DATA_PATH}/${IN_META_FILE}
    echo "type=input" >> ${LOCAL_DATA_PATH}/${IN_META_FILE}
    echo "[server]" >> ${LOCAL_DATA_PATH}/${IN_META_FILE}
    echo "file=${infile}" >> ${LOCAL_DATA_PATH}/${IN_META_FILE}
    echo "host=${server_addr}" >> ${LOCAL_DATA_PATH}/${IN_META_FILE}
    echo "port=${server_port}" >> ${LOCAL_DATA_PATH}/${IN_META_FILE}
    echo "compress=false" >> ${LOCAL_DATA_PATH}/${IN_META_FILE}
    echo "block_size=${blocksize}" >> ${LOCAL_DATA_PATH}/${IN_META_FILE}
    #echo "save_local=false" >> ${LOCAL_DATA_PATH}/${IN_META_FILE}
    echo "prefetch=false" >> ${LOCAL_DATA_PATH}/${IN_META_FILE}

    echo "TAZER0.1" | tee ${LOCAL_DATA_PATH}/${OUT_META_FILE}
    echo "type=output" >> ${LOCAL_DATA_PATH}/${OUT_META_FILE}
    echo "[server]" >> ${LOCAL_DATA_PATH}/${OUT_META_FILE}
    echo "file=${writefile}" >> ${LOCAL_DATA_PATH}/${OUT_META_FILE}
    echo "host=${server_addr}" >> ${LOCAL_DATA_PATH}/${OUT_META_FILE}
    echo "port=${server_port}" >> ${LOCAL_DATA_PATH}/${OUT_META_FILE}
    echo "compress=false" >> ${LOCAL_DATA_PATH}/${OUT_META_FILE}
    echo "block_size=${blocksize}" >> ${LOCAL_DATA_PATH}/${OUT_META_FILE}
    #echo "save_local=false" >> ${LOCAL_DATA_PATH}/${OUT_META_FILE}
    echo "prefetch=false" >> ${LOCAL_DATA_PATH}/${OUT_META_FILE}
    out_file=./log.out

    t=$(date +%s)
    var_names="${var_names},InputDataSet" && var_vals="${var_vals},${fnum}" && var_times="${var_times},${t}"

    t=$(date +%s)
    var_names="${var_names},StartInputTx" && var_vals="${var_vals},${t}" && var_times="${var_times},${t}"

    t=$(date +%s)
    var_names="${var_names},StopInputTx" && var_vals="${var_vals},${t}" && var_times="${var_times},${t}"

    t=$(date +%s)
    var_names="${var_names},StartExp" && var_vals="${var_vals},${t}" && var_times="${var_times},${t}"


    echo "TAZER_SHARED_MEM_CACHE=${CACHES[0]} TAZER_SHARED_MEM_CACHE_SIZE=$((${CACHE_SIZES[0]})) \
    TAZER_BB_CACHE=${CACHES[1]} TAZER_BB_CACHE_SIZE=$((${CACHE_SIZES[1]})) \
    TAZER_BOUNDED_FILELOCK_CACHE=${CACHES[2]} TAZER_BOUNDED_FILELOCK_CACHE_SIZE=$((${CACHE_SIZES[2]}))"
    ref_time=0 #debug -- calcuate a reference time (e.g. with SimplePTP if you want to use)
    echo "ref_time ${ref_time}"
    #time gdb --ex run --ex bt --args env
    #time gdb --ex run --ex "thread apply all bt" --args env

    time TAZER_REF_TIME=${ref_time} TAZER_SHARED_MEM_CACHE=$((${CACHES[0]})) TAZER_SHARED_MEM_CACHE_SIZE=$((${CACHE_SIZES[0]})) \
    TAZER_BB_CACHE=$((${CACHES[1]})) TAZER_BB_CACHE_SIZE=$((${CACHE_SIZES[1]})) \
    TAZER_BOUNDED_FILELOCK_CACHE=${CACHES[2]} TAZER_BOUNDED_FILELOCK_CACHE_SIZE=$((${CACHE_SIZES[2]})) \
    TAZER_PREFETCH=0 LD_PRELOAD=${TAZER_LIB} ${MULTITHREADED_TEST_PATH} ${NUM_THREADS} ${LOCAL_DATA_PATH}/${IN_META_FILE} ${LOCAL_DATA_PATH}/${OUT_META_FILE} ${workspace}/test_${test_id} >& ${output_path}/tazer_output.txt

    
    # /share/apps/gcc/8.1.0/lib64/libasan.so:
    #LD_PRELOAD=/share/apps/gcc/8.1.0/lib64/libasan.so:${TAZER_LIB}

    t=$(date +%s)
    var_names="${var_names},StopExp" && var_vals="${var_vals},${t}" && var_times="${var_times},${t}"

    parsed=`python ${tazer_path}/utils/plot_thread_times/ParseTazerOutput.py ${output_path}/tazer_output.txt`
    tmp_names=`echo "$parsed" | grep -oP '(?<=labels:).*'` 
    tmp_vals=`echo "$parsed" | grep -oP '(?<=vals:).*'` 
    var_names="${var_names},${tmp_names}" && var_vals="${var_vals},${tmp_vals}"

    #rm -r $data_dir

    t=$(date +%s)
    var_names="${var_names},FinishedTime" && var_vals="${var_vals},${t}" && var_times="${var_times},${t}"
    echo "$MY_HOSTNAME=$JOBID=$JOBID;$var_names;$var_vals" >& ${output_path}/parsed_output.txt

    python ${tazer_path}/utils/plot_thread_times/plot_thread_times.py ${output_path}/parsed_output.txt ${output_path}/thread_times

}

exit 0