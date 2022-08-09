#!/bin/bash
module purge
# module load gcc/9.1.0
module load gcc/10.3.0
# module load gcc/8.1.0
module load python/3.7.0
# conda init bash
source /share/apps/python/miniconda3.7/etc/profile.d/conda.sh
conda activate /files0/oddite/conda/ddmd/

#!!!!!! CHANGE THIS FOLDER !!!!!!
SCRIPT_DIR=/qfs/people/firo017/tazer/tazer_2/tazer/script/
RES_DIR=$SCRIPT_DIR/"Results_ddmd_trackfile/"
mkdir $RES_DIR

test_list=("ddmd_sim_test")

test_memory=(64 64) 

num_tests=${#test_list[@]}
ulimit -c unlimited


for scalable in 0; do
    for (( test_no=0 ; test_no<=$num_tests-1 ; test_no++ ));  do
        export TAZER_SCALABLE_CACHE=${scalable}
        export TAZER_PRIVATE_MEM_CACHE_SIZE=$((test_memory[$test_no]*1024*1024))
        export TAZER_SCALABLE_CACHE_NUM_BLOCKS=${test_memory[$test_no]}

        test_name=${test_list[$test_no]}
        echo $test_name
    
        mkdir $RES_DIR/${test_name}_${scalable}
        cd $RES_DIR/${test_name}_${scalable}

        # cp ../../*.meta.in ./
        # cp ../../*.meta.out ./
        # cp ../../tazer3_output.dat ./
	# ln -s ../../tazer.dat ./tazer.dat
        # ln -s ../../tazer2.dat ./tazer2.dat
        # ln -s ../../tazer3.dat ./tazer3.dat
        cp ../../runTest.pl ./
        cp ../../runScale_ddmd.pl ./

        ./runScale_ddmd.pl > ${test_name}_${scalable}.out 2>&1 &
    done
done


