#!/bin/bash

#This file is called directly by the jenkins pipeline script. It starts a tazer server and starts the TazerCp tests
#and the Workflow Simulation tests found in combined-testing.
TAZER_WORKSPACE_ROOT=$GITHUB_WORKSPACE

if [ -z "$TAZER_WORKSPACE_ROOT" ];  then
TAZER_WORKSPACE_ROOT=`pwd` #assumes it was launched locally from tazer root
fi

TAZER_BUILD_DIR=$1
if [ -z "$TAZER_BUILD_DIR" ];  then
    TAZER_BUILD_DIR=build
fi

cd tests/integration_tests

module load gcc/8.1.0 python/3.7.0

#Edit these values (total_client_nodes and total_clients_per)
#Each node will have a certain number of clients running tests on it. Half of a given nodes clients will run
#workflow sim tests while the other half runs TazerCp tests.
total_client_nodes=3
total_clients_per=2


workspace=$TAZER_WORKSPACE_ROOT/runner-test/integration
#Start the tazer server on a node and sleep for a while to be sure that the server has time to create its 1GB data file and 
#start up before the clients start trying to use it.
tazer_server_task_id=`sbatch --parsable -N1 start_tazer_server.sh $workspace $TAZER_WORKSPACE_ROOT $TAZER_BUILD_DIR`
tazer_server_nodes=`squeue -j ${tazer_server_task_id} -h -o "%N"`
while [ -z "$tazer_server_nodes" ]; do
tazer_server_nodes=`squeue -j ${tazer_server_task_id} -h -o "%N"`
done
$TAZER_WORKSPACE_ROOT/${TAZER_BUILD_DIR}/test/PingServer $tazer_server_nodes 5001 300 1
sbatch --wait --parsable -N ${total_client_nodes} launch_tazer_clients.sh ${workspace} ${TAZER_WORKSPACE_ROOT} ${TAZER_BUILD_DIR} ${tazer_server_nodes} ${total_clients_per}

# #using sbatch --wait wasn't working for larger tests so I made this loop which waits until every client has returned its info.
# clients_finished=0
# total_clients=$(($total_client_nodes*$total_clients_per))
# sleep_time=$((60*5))
# while [ $clients_finished -lt $total_clients ]
# do
#     clients_finished=`ls -d client[0-9]* 2> /dev/null | wc -l`
#     if [ $clients_finished -gt 0 ]; then
#         sleep_time=60
#     fi
#     sleep $sleep_time
#     echo "waiting for tests to finish: $clients_finished/$total_clients"
# done

#After the tests finish we need to parse through their information and convert that info into a junit style xml file
#the file is placed in test_results and used by jenkins to report test results.
echo "Closing server ..."

$TAZER_WORKSPACE_ROOT/${TAZER_BUILD_DIR}/test/CloseServer $SERVER_NODE 5001
./report_results.sh $workspace/client

sleep 10

exit 0
