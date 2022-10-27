#!/bin/bash
module purge
module load gcc/8.1.0
module load python/3.7.0

#!!!!!! CHANGE THIS FOLDER !!!!!!
SCRIPT_DIR=/people/mutl832/tazer-july/script
RES_DIR=$SCRIPT_DIR/"Results-oldcost/"
mkdir $RES_DIR

#test_list=("Test1A" "Test1B" "Test1C_rr" "Test1C_l1r4" "Test1C_l5r1" "NEW_Test2A_linear" "NEW_Test2B_random")
test_list=("Test1A" "Test1B" "Test1C_rr" "Test1C_l1r4" "Test1C_l5r1")
#test_list=("Test1C_rr" "Test1C_l1r4" "Test1C_l5r1" "NEW_Test2A_linear" "NEW_Test2B_random")
#test_memory=(64 64 64 64 64 60 60)
test_list=("Workload2-A" "Workload2-B" "Workload2-C" "Workload2-D" "Workload2-E" "Workload2-F" "Workload2-G" "Workload2-H")
#test_memory=(16 16 16 16 16 16 16 16)
test_list=("Workload2-F" "Workload2-B" )
test_list=("Workload2-B")
test_memory=(32 32 32 32 32 32 32 32)
num_tests=${#test_list[@]}

export TAZER_Hb_VALUE=100
export TAZER_H_VALUE=0
export TAZER_SHARED_MEM_CACHE=1
export TAZER_SHARED_MEM_CACHE_SIZE=$((32*1024*1024))
export TAZER_BOUNDED_FILELOCK_CACHE=0
export TAZER_BLOCKSIZE=$((1024*1024))
##below parameters doesnt matter for private cache 
export TAZER_MC_VALUE=0
export TAZER_Sr_VALUE=0
export TAZER_UMB_THRESHOLD=50

for scalable in 1; do
    for (( test_no=0 ; test_no<=$num_tests-1 ; test_no++ ));  do
        export TAZER_SCALABLE_CACHE=${scalable}
        export TAZER_PRIVATE_MEM_CACHE_SIZE=$((test_memory[$test_no]*1024*1024))
        export TAZER_SCALABLE_CACHE_NUM_BLOCKS=${test_memory[$test_no]}

        test_name=${test_list[$test_no]}
        echo $test_name
    
        mkdir $RES_DIR/${test_name}_${scalable}
        cd $RES_DIR/${test_name}_${scalable}

        cp ../../*.meta.in ./
        ln -s ../../tazer.dat ./tazer.dat
        ln -s ../../tazer2.dat ./tazer2.dat
        ln -s ../../tazer3.dat ./tazer3.dat
        cp ../../runTest.pl ./
        cp ../../runScale.pl ./

        ./runScale.pl ../../paper_experiments/${test_name}.txt > ${test_name}_${scalable}.out 2>&1 &
        # if [ $scalable -eq 0 ] 
        # then
        #     python ../../PlotPartitionChange.py ${test_name}_${scalable}.out ${test_name}_${scalable}.png
        # fi
    done
done


# export TAZER_PRIVATE_MEM_CACHE_SIZE=60
# export TAZER_SCALABLE_CACHE_NUM_BLOCKS=60
# export TAZER_SCALABLE_CACHE=1
# `cp ../../runTest.pl ./`
# `cp ../../runScale.pl ./`
# `cp ../../tazer.dat.meta.in ./`
# `cp ../../tazer2.dat.meta.in ./`
# `cp ../../tazer3.dat.meta.in ./`

# `./runScale.pl ../../paper_experiments/${test_name}.txt > ${test_name}.out 2>&1`
# `python ../../PlotPartitionChange.py ${test_name}.out ${test_name}.png`


