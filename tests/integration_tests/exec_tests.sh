#!/bin/bash

#This tests tazer_client_workflow_test.sh with different combinations of cache sizes on the three caches used in that test, shared memory
#burst buffer, and bounded filelock. Every combination of the given cache sizes is tested twice in order to utilize the caches.
#These cache size tests are run once for every given set of File_access_pattern_gen.py parameters.
ref_time=`date +%s%N`
global_cache_path=0

workspace=$1
data_path=$2
tazer_path=$3
build_dir=$4
server_addr=$5
server_port=$6
global_cache_path=$7
simdir=$8
shmem=$9
burstbuff=${10}
filelock=${11}
test_id=${12}

# echo "###### EXEC_TEST ######"
# echo "$@"
# echo "$0 $1 $2 $3 $4 $5 $6 $7 $8 $9 ${10} ${11} ${12}"

node=`hostname`
node_workspace=${workspace}/client/${node}
mkdir -p ${node_workspace}
cd ${node_workspace}


integration_test_path=${tazer_path} #/tests/integration_tests
tazer_cp_read=${integration_test_path}/tazer_cp_read_test_client.sh
tazer_cp_write=${integration_test_path}/tazer_cp_write_test_client.sh
workflow_test=${integration_test_path}/tazer_client_workflow_test.sh


#clean up previous caches
rm -r /tmp/*tazer${USER}*
rm -r /state/partition1/*tazer${USER}*
rm /dev/shm/*tazer${USER}*


#prepare caches
mkdir -p /tmp/tazer${USER}/tazer_cache
#global filelock
ln -s $global_cache_path /tmp/tazer${USER}/tazer_cache/
#burst_buffer
mkdir -p /state/partition1/tazer${USER}/tazer_cache/bbc
ln -s /state/partition1/tazer${USER}/tazer_cache/bbc  /tmp/tazer${USER}/tazer_cache/

#create temp files


IO_RATES=( 125 )
NUM_CORES=( 10 )
TASKS_PER_CORE=(1)
SEGMENT_SIZES=($((0)) $((8*1024*1024)))
READ_PROBS=(.75)
NUM_CYCLES=(1 4) #(1 4)
MAX_FILE_SIZES=( $((2*1024*1024*1024)) )
READ_SIZES=( $((8*1024))  $((300*1024)))
EXEC_TIMES=(30) #30
#ACCESS_PATTERN=( "--random" "--random" )
ACCESS_PATTERN=( "" "--random" )

MY_NODE=$SLURM_PROCID
NUM_NODES=$SLURM_NPROCS


if [ "$test_id" = "0" ]; then
test_id=$((starting_test+MY_NODE))
failed=0

else 
. temp.vals
test_id=$TESTID
failed=$FAILED
fi

starting_test=$test_id

echo "&&&&&&& TESTID ${test_id} ${starting_id} ${MY_NODE} ${NUM_NODES} clock drift: ${clock_drift}"



files_1=(${data_path}/*.dat)
#test_pids=()
#$tazer_cp_write ${node_workspace} ${data_path} ${tazer_path} ${build_dir} ${server_addr} ${server_port} ${test_id} ${ref_time} &
#test_pids+=("$!")
#test_id=$((test_id+NUM_NODES))
#$tazer_cp_read ${node_workspace} ${data_path} ${tazer_path} ${build_dir} ${server_addr} ${server_port} ${test_id} ${shmem} ${burstbuff} ${filelock} ${ref_time} &
#test_pids+=("$!")
#test_id=$((test_id+NUM_NODES))


# echo " ${IO_RATES[@]} --- ${NUM_CORES[@]} --- ${TASKS_PER_CORE[@]} --- ${SEGMENT_SIZES[@]} --- ${READ_PROBS[@]} --- ${NUM_CYCLES[@]} --- ${READ_SIZES[@]} --- ${EXEC_TIMES[@]} ---"
index=0
for io_rate in "${IO_RATES[@]}"; do
    for num_cores in "${NUM_CORES[@]}"; do
        for tpc in "${TASKS_PER_CORE[@]}"; do
            for seg_size in "${SEGMENT_SIZES[@]}"; do
                for prob in "${READ_PROBS[@]}"; do
                    for cyc in "${NUM_CYCLES[@]}"; do
                        # for mfs in ${MAX_FILE_SIZES[@]}; do
                            for rdsz in "${READ_SIZES[@]}"; do
                                for t in "${EXEC_TIMES[@]}"; do
                                    for pat in "${ACCESS_PATTERN[@]}"; do
                                        for i in {0..1}; do
size=${#files_1[@]}
#index=$(($RANDOM % $size))
h=${files_1[$index]}
a=`echo $h | awk -F'/' '{print $NF}'` 
SUB="test"
if [[ "$a" != *"$SUB"* ]]; then
$workflow_test ${node_workspace} ${data_path} ${tazer_path} ${build_dir} ${server_addr} ${server_port} ${test_id} ${simdir} "--ioRate=${io_rate} --numCores=${num_cores} --tasksPerCore=${tpc} --segmentSize=${seg_size}  --readProbability=${prob} --numCycles=${cyc} --readSize=${rdsz} --execTime=${t} ${pat}" ${shmem} ${burstbuff} ${filelock} ${ref_time}  ${a} &
test_pids+=("$!")
test_id=$((test_id+NUM_NODES))
fi
index=$((index+1))
index=$((index%size))
                                        done
                                    done
                                done
                            done
                        # done
                    done
                done
            done
        done
    done
done


temp_test_id=$starting_test;
for job in "${test_pids[@]}" ; do
    wait ${job} 
    status=$?
    if [ "$status" -ne "0" ]; then
        echo "FAILED: ${node} test ${temp_test_id}"
        failed=$((failed+1))
    fi
    temp_test_id=$((temp_test_id+NUM_NODES))
done

echo "TESTID=$test_id
      FAILED=$failed" > temp.vals


echo "Total Tests: ${#test_pids[@]} Failed Tests: ${failed}"
