#!/bin/bash

#This tests tazer_cp_test_client.sh with different combinations of cache sizes on the three caches used in that test, shared memory
#burst buffer, and bounded filelock. The CACHE_SIZES variable must contain a list of cache sizes. Every combination of
#the given cache sizes is tested twice in order to utilize the caches.

workspace=$1
tazer_path=$2
build_dir=$3
server_addr=$4
client_id=$5
echo "$0 $1 $2 $3 $4 $5"
node=`hostname`

client_name=client_${client_id}
mkdir ${workspace}/${client_name}
cd ${workspace}/${client_name}

OUTPUT="${client_name}.out"
echo "${node}" > $OUTPUT
echo "${client_name}" >> $OUTPUT
ERROR="${client_name}.error"
echo "${node}" > $ERROR
echo "${client_name}" >> $ERROR

echo "export WAITING=0" > shared_vars.txt
echo "export DONE=0" >> shared_vars.txt
echo "export FAILED=0" >> shared_vars.txt

build_path=${tazer_path}/${build_dir}
integration_test_path=$tazer_path/tests/integration_tests
tazer_cp_read="${integration_test_path}/tazer_cp_read_test_client.sh"
tazer_cp_write="${integration_test_path}/tazer_cp_write_test_client.sh"

#Change the CACHE_SIZES variable to contain a list of whatever sizes you want to test.
CACHE_SIZES=(0 256*1024*1024  2*1024*1024*1024)
num_sizes=${#CACHE_SIZES[@]}
#CACHE_SIZES=(1024*1024*1024)
TOTAL_TESTS=$((num_sizes**3)) #using three levels of caches
TOTAL_TESTS=$((TOTAL_TESTS+1)) #one test for writing
TOTAL_TESTS=$((TOTAL_TESTS*2)) #2 tests per cache configuration
TOTAL_TESTS_RUN=0

echo "export TEST_USED=TazerCp" >> shared_vars.txt

#This function is called by the loops below. It runs tazer_cp_test_client.sh twice using the same cache sizes.
#Afterward it changes the WAITING variable in the client's shared_vars.txt file. This file is read by this client's node
#in launch_tazer_client.sh which deletes /dev/shm/*tazer* and /tmp/*tazer* after every client on the node is ready.
#if one of the tests returns non 0 the DONE and FAILED variables in the client's shared_vars.txt will be set to 1 and
#it will exit 1. If one client fails and returns before all tests finish, the others will not have to wait for it when waiting for cleanup.
do_test()
{
    TOTAL_TESTS_RUN=$((TOTAL_TESTS_RUN+1))
    echo "##### TazerCp Test $TOTAL_TESTS_RUN/$TOTAL_TESTS #####" >> $OUTPUT
    $tazer_cp_read ${workspace}/${client_name} ${tazer_path} ${build_dir} $server_addr $1 $2 $3 >> $OUTPUT 2>>$ERROR
    if [ ! $? == 0 ]; then
        echo "$0 failed" >> $ERROR
        echo "test failed: TazerCp Test $TOTAL_TESTS_RUN/$TOTAL_TESTS" >> $ERROR
        sed -i "s/DONE=0/DONE=1/g" shared_vars.txt
        sed -i "s/FAILED=0/FAILED=1/g" shared_vars.txt
        echo "export TESTS_RUN=$TOTAL_TESTS_RUN" >> shared_vars.txt
        exit 1
    fi

    TOTAL_TESTS_RUN=$((TOTAL_TESTS_RUN+1))
    echo "##### Test $TOTAL_TESTS_RUN/$TOTAL_TESTS #####" >> $OUTPUT
    $tazer_cp_read ${workspace}/${client_name} ${tazer_path} ${build_dir} $server_addr $1 $2 $3 >> $OUTPUT 2>>$ERROR
    if [ ! $? == 0 ]; then
        echo "$0 failed" >> $ERROR
        echo "test failed: TazerCp Test $TOTAL_TESTS_RUN/$TOTAL_TESTS" >> $ERROR
        sed -i "s/DONE=0/DONE=1/g" shared_vars.txt
        sed -i "s/FAILED=0/FAILED=1/g" shared_vars.txt
        echo "export TESTS_RUN=$TOTAL_TESTS_RUN" >> shared_vars.txt
        exit 1
    fi

    #wait for cleanup
    sed -i "s/WAITING=0/WAITING=1/g" shared_vars.txt
    . shared_vars.txt
    echo "$client_name finished TazerCp Test $TOTAL_TESTS_RUN/$TOTAL_TESTS" >> $OUTPUT
    #launch_tazer_client.sh will change the value of WAITING after it performs the cleanup step
    while [ $WAITING == 1 ]
    do
        . shared_vars.txt
        sleep 1
    done
    echo "$client_name done waiting for cleanup" >> $OUTPUT
}

#First do a test copying local file to remote server. This test is only run one time because it doesn't use the 
#three caches that are normally tested.
TOTAL_TESTS_RUN=$((TOTAL_TESTS_RUN+1))
echo "##### TazerCp write from local to remote test  $TOTAL_TESTS_RUN/$TOTAL_TESTS} #####" >> $OUTPUT
$tazer_cp_write ${workspace}/${client_name} ${tazer_path} ${build_dir} ${server_addr}  ${client_name} >> $OUTPUT 2>>$ERROR
if [ ! $? == 0 ]; then
    echo "$0 failed" >> $ERROR
    echo "test failed: TazerCp write from local to remote test " >> $ERROR
    sed -i "s/DONE=0/DONE=1/g" shared_vars.txt
    sed -i "s/FAILED=0/FAILED=1/g" shared_vars.txt
    echo "export TESTS_RUN=$TOTAL_TESTS_RUN" >> shared_vars.txt
    exit 1
fi

#These for loops should cover every possible combination of the three caches and the given cache sizes.
for shared_mem in ${CACHE_SIZES[@]}
do
    for burst_buffer in ${CACHE_SIZES[@]}
    do
        for bounded_filelock in ${CACHE_SIZES[@]}
        do
            do_test $shared_mem $burst_buffer $bounded_filelock
        done
    done
done



#The node's launch_tazer_client.sh continuously checks all of its clients' shared_vars.txt files. Once they all have
#DONE set to 1, launch_tazer_client.sh can do some final steps and exit.
echo "export TESTS_RUN=$TOTAL_TESTS_RUN" >> shared_vars.txt
sed -i "s/DONE=0/DONE=1/g" shared_vars.txt
