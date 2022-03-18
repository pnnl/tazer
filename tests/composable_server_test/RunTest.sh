#!/bin/bash

#run this script to run the multithreaded_tests, edit multithreaded_test.sh to change the number of threads
#TAZER_WORKSPACE_ROOT=$GITHUB_WORKSPACE
TAZER_WORKSPACE_ROOT=/people/powe445/Projects/update_tazer_file_format/tazer

if [ -z "$TAZER_WORKSPACE_ROOT" ];  then
TAZER_WORKSPACE_ROOT=`pwd` #assumes it was launched locally from tazer root
fi

TAZER_BUILD_DIR=$1
if [ -z "$TAZER_BUILD_DIR" ];  then
    TAZER_BUILD_DIR=build
fi

cd $TAZER_WORKSPACE_ROOT/tests/composable_server_test

module load gcc/8.1.0 python/3.7.0

#make

total_client_nodes=1

workspace=$TAZER_WORKSPACE_ROOT/runner-test/composable_server_test

data_path=${workspace}/tazer_data
mkdir -p $data_path
rm $data_path/test*.dat
if [ ! -f $data_path/tazer1GB.dat ]; then
    dd if=/dev/urandom of=$data_path/tazer1GB.dat bs=10M count=100 &
fi

main_server_port=5001
tazer_server_task_id=`sbatch -A ippd --parsable --exclude=node04,node33,node23,node24,node43 -N1 start_main_server.sh $workspace $TAZER_WORKSPACE_ROOT $TAZER_BUILD_DIR $main_server_port`
main_server_node=`squeue -j ${tazer_server_task_id} -h -o "%N"`
while [ -z "$main_server_node" ]; do
main_server_node=`squeue -j ${tazer_server_task_id} -h -o "%N"`
done

tazer_server_port=5001
tazer_server_task_id=`sbatch -A ippd --parsable --exclude=node04,node33,node23,node24,node43 -N1 start_middle_server.sh $workspace $TAZER_WORKSPACE_ROOT $TAZER_BUILD_DIR $tazer_server_port $main_server_node $main_server_port`
middle_server_node=`squeue -j ${tazer_server_task_id} -h -o "%N"`
while [ -z "$middle_server_node" ]; do
middle_server_node=`squeue -j ${tazer_server_task_id} -h -o "%N"`
done

$TAZER_WORKSPACE_ROOT/${TAZER_BUILD_DIR}/test/PingServer $main_server_node $main_server_port 300 1
#$TAZER_WORKSPACE_ROOT/${TAZER_BUILD_DIR}/test/PingServer $middle_server_node $tazer_server_port 300 1

wait #need to wait for the temp files to finish being created

sbatch -A ippd --wait --exclude=node04,node33,node23,node24,node43 -N ${total_client_nodes} launch_tazer_clients.sh ${workspace} ${data_path} ${TAZER_WORKSPACE_ROOT} ${TAZER_BUILD_DIR} ${middle_server_node} ${tazer_server_port} 

echo "Closing server ..."

$TAZER_WORKSPACE_ROOT/${TAZER_BUILD_DIR}/test/CloseServer $main_server_node $main_server_port 5001
$TAZER_WORKSPACE_ROOT/${TAZER_BUILD_DIR}/test/CloseServer $middle_server_node $tazer_server_port 5001