#!/bin/bash

#This script runs on each client node, it starts a certain number of clients on its node, half using run_workflow_tests.sh
#and the other half using run_tazer_cp_tests.sh.  It continuously checks if all of its clients are ready for cleanup,
#once they are all ready (happens after every other test) it removes /dev/shm/*tazer* and /tmp/*tazer*.
workspace=$1
node=`hostname`
node_workspace=${workspace}/client/${node}
mkdir -p ${node_workspace}
cd ${node_workspace}
rm -r client*

echo "$node"

tazer_path=$2
build_dir=$3
server_nodes=$4
num_clients=$5
integration_test_path=$tazer_path/tests/integration_tests

echo "$0 $1 $2 $3 $4 $5"

SERVER_ADDR=$server_nodes #this currently assumes just a single server...
SERVER_PORT="5001"



CLEANUP=0
FINISHED=0

workflow_sim="${integration_test_path}/run_workflow_tests.sh"
tazer_cp="${integration_test_path}/tazer_cp_tests.sh"




#Each client is given a directory on its node, each containing a shared_vars.txt that the client will use to share info.
#check_client_files() checks all of those shared_vars.txt files to see if the clients are waiting for cleanup ($WAITING).
#It also checks to see if the clients are completly done with all of their tests ($DONE).
check_client_files()
{
    client_id=0
    while [ $client_id -lt $num_clients ]
    do
        . ${node_workspace}/client_${client_id}/shared_vars.txt
        if [ "$WAITING" = "1" ]; then
            CLEANUP=$(($CLEANUP+1))
        fi
        if [ "$DONE" = "1" ]; then
            FINISHED=$(($FINISHED+1))
            if [ ! "$WAITING" = "1" ]; then
                CLEANUP=$(($CLEANUP+1))
            fi
        fi
        client_id=$((client_id+1))
    done
}
#This function is called after performing cleanup to reset WAITING in the clients' shared_vars.txt files.
reset_client_files()
{
    client_id=0
    while [ $client_id -lt $num_clients ]
    do
        sed -i "s/WAITING=1/WAITING=0/g" ${node_workspace}/client_${client_id}/shared_vars.txt
        client_id=$((client_id+1))
    done
}

#This loop starts all of the node's clients and creates their client directories and necessary files like the
#stdout file called client#.out, stderr file called client#.error, and a shared_vars.txt on which the client 
#will write info. This shared_vars.txt is later read in order to determine if a client is ready for cleanup, or 
#completly finished with its tests. It is also used to provide info for report_results.sh.
client_id=0
flag=0
while [ $client_id -lt $num_clients ]
do
    $tazer_cp ${node_workspace} ${tazer_path} ${build_dir} ${SERVER_ADDR} ${client_id} &
    
    #flag is used to alternate between TEST1 and TEST2 to ensure that half of the clients do run_workflow_tests.sh and half do run_tazer_cp_tests.sh.
    # if [ $flag == 0 ]; then
    #     $TEST1 $SERVER_NODE "$CLIENT_NAME" "$OUTFILE" "$ERRORFILE" $combined_testing_path &
    #     flag=1
    # else
    #     CLIENT_DATA_FILE="client$j.dat"
    #     $TEST2 $SERVER_NODE "$CLIENT_NAME" "$CLIENT_DATA_FILE" "$OUTFILE" "$ERRORFILE" $combined_testing_path &
    #     flag=0
    # fi
    # cd ..
    client_id=$(($client_id+1))
done

#After all of the clients have started this loop uses check_client_files() and reset_client_files() to check if all of
#the clients are ready to remove /dev/shm/*tazer* and /tmp/*tazer*. When all of the clients are finished testing it exits the loop.
while [ $FINISHED -lt $num_clients ] 
do
    CLEANUP=0
    FINISHED=0
    check_client_files
    if [ "$CLEANUP" = "$num_clients" ]; then
        echo "Doing cleanup"
        echo "Node $node doing cleanup for clients"
        rm /dev/shm/*tazer*
        rm -r /tmp/*tazer*
        reset_client_files
    fi
    sleep 10
done

#Once the clients are done we need to copy their info files up to the tazer-jenkins-workspace so they can be seen and 
#used by the report_results.sh and so the final node (see below) can check if all of the client nodes have finished.
# client_id=0
# while [ $client_id -lt $num_clients ]
# do
#     mkdir -p ${workspace}/client_$client_id
#     cp client$i/client$i.out $combined_testing_path/client$i
#     cp client$i/client$i.error $combined_testing_path/client$i
#     cp client$i/shared_vars.txt $combined_testing_path/client$i
#     i=$(($i+1))
# done

echo "Finished"

exit 0
