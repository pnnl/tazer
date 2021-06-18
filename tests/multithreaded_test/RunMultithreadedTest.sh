#!/bin/bash

#run this script to run the multithreaded_tests, edit multithreaded_test.sh to change the number of threads
#TAZER_WORKSPACE_ROOT=$GITHUB_WORKSPACE
TAZER_WORKSPACE_ROOT=/people/powe445/Projects/tazer_report_thread_stats/tazer

if [ -z "$TAZER_WORKSPACE_ROOT" ];  then
TAZER_WORKSPACE_ROOT=`pwd` #assumes it was launched locally from tazer root
fi

TAZER_BUILD_DIR=$1
if [ -z "$TAZER_BUILD_DIR" ];  then
    TAZER_BUILD_DIR=build
fi



cd tests/multithreaded_test

module load gcc/8.1.0 python/3.7.0


total_client_nodes=2
total_clients_per=5


workspace=$TAZER_WORKSPACE_ROOT/runner-test/multithreaded_test

# create test data we will use to transfer
# launch in the background as these can be time comsuming
# we will wait just before launching the actual tests
data_path=${workspace}/tazer_data
mkdir -p $data_path
#Remove files that were copied from clients to this node in previous tests (tazer_cp_write_test_client.sh).
rm $data_path/test*.dat


if [ ! -f $data_path/tazer100MB.dat ]; then
    
    dd if=/dev/urandom of=$data_path/tazer100MB.dat bs=10M count=10 &
fi


tazer_server_port=5001
#Start the tazer server on a node and sleep for a while to be sure that the server has time to create its 1GB data file and 
#start up before the clients start trying to use it.
tazer_server_task_id=`sbatch -A ippd --parsable --exclude=node04,node33,node23,node24,node43 -N1 start_tazer_server.sh $workspace $TAZER_WORKSPACE_ROOT $TAZER_BUILD_DIR $tazer_server_port`
tazer_server_nodes=`squeue -j ${tazer_server_task_id} -h -o "%N"`
while [ -z "$tazer_server_nodes" ]; do
tazer_server_nodes=`squeue -j ${tazer_server_task_id} -h -o "%N"`
done

$TAZER_WORKSPACE_ROOT/${TAZER_BUILD_DIR}/test/PingServer $tazer_server_nodes $tazer_server_port 300 1

wait #need to wait for the temp files to finish being created

sbatch -A ippd --wait --exclude=node04,node33,node23,node24,node43 -N ${total_client_nodes} launch_tazer_clients.sh ${workspace} ${data_path} ${TAZER_WORKSPACE_ROOT} ${TAZER_BUILD_DIR} ${tazer_server_nodes} ${tazer_server_port} ${total_clients_per} ${total_client_nodes} 

echo "Closing server ..."

$TAZER_WORKSPACE_ROOT/${TAZER_BUILD_DIR}/test/CloseServer $tazer_server_nodes $tazer_server_port 5001
total_tests=0
total_failed=0

for node in `ls ${workspace}/client`; do
. ${workspace}/client/$node/temp.vals
if [ $TESTID -gt $total_tests ]; then
total_tests=$TESTID
fi
total_failed=$((total_failed + FAILED))
done
echo "Total Tests: ${total_tests} Failed Tests: ${total_failed}"


sleep 10

if  [ "${total_failed}" != "0" ]; then
exit 1
else
exit 0
fi