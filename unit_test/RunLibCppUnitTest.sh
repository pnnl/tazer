#!/bin/bash

#This file is called directly by the github actions workflow script. It starts a tazer server on one node, and then
#starts another node with a script that will do more setup, and run the Lib.cpp unit test
TAZER_WORKSPACE_ROOT=$GITHUB_WORKSPACE

if [ -z "$TAZER_WORKSPACE_ROOT" ];  then
TAZER_WORKSPACE_ROOT=`pwd` #assumes it was launched locally from tazer root
fi

TAZER_BUILD_DIR=$1
if [ -z "$TAZER_BUILD_DIR" ];  then
    $TAZER_BUILD_DIR=build
fi


echo "$TAZER_WORKSPACE_ROOT $TAZER_BUILD_DIR"

# mv $TAZER_JENKINS_WORKSPACE/libcpp_unit_test $TAZER_JENKINS_WORKSPACE/tazer
# cd $TAZER_JENKINS_WORKSPACE/tazer/libcpp_unit_test
cd unit_test

workspace=$TAZER_WORKSPACE_ROOT/runner-test
# tazer_path=`pwd`/..

# sbatch -N1 start_tazer_server.sh $workspace $TAZER_WORKSPACE_ROOT
tazer_server_task_id=`sbatch --parsable -N1 start_tazer_server.sh $workspace $TAZER_WORKSPACE_ROOT $TAZER_BUILD_DIR`
tazer_server_nodes=`squeue -j ${tazer_server_task_id} -h -o "%N"`
while [ -z "$tazer_server_nodes" ]; do
tazer_server_nodes=`squeue -j ${tazer_server_task_id} -h -o "%N"`
done
echo "id: $tazer_server_task_id server_nodes: $tazer_server_nodes"
sbatch --wait --dependency after:${tazer_server_task_id} -N1 run_unit_test.sh $workspace $TAZER_WORKSPACE_ROOT $TAZER_BUILD_DIR $tazer_server_nodes

cat ${workspace}/client/test_results/lib_unit_test_out.txt
