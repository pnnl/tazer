#!/bin/bash

# need to create a list of files in their current locations


# NOTE TO SELF: If I change either the execTime or the ioRate, I need to increase the number of cores and or the number of tasks per cores

my_array=(inter strided batched)
vals=($(seq 1 1 10))
loc=$1
data=$2 #/people/belo700/speedracer-tazer-bigflowsim/block_temp/tazer/tests/integration_tests/runner-test/integration/tazer_data

for i in "${my_array[@]}"; 
do 
    for j in "${vals[@]}";
    do
    rand=$(( $RANDOM % 2 + 1 ))
    readperc=`seq 0.5 .01 1 | shuf | head -n1`
    incinread=`seq 1 1 50 | shuf | head -n1`
    incinwrite=`seq 1 1 50 | shuf | head -n1`
    echo "The read percentage is equal to $readperc "
    echo "The read modifier is equal to $incinread "
    echo "The write modifier is equal to $incinwrite "

    if [ $rand -gt 1 ]
    then 
        python $loc/File_access_pattern_gen.py --ioRate=125 --numCores=$((10*$j)) --tasksPerCore=1 --execTime=$((30*$j)) --segmentSize=0 --numCycles=1 --readProbability=$readperc --readSize=$(($incinread*16*1024)) --outputSize=$(($incinwrite*16*1024)) --outputPattern=`echo $i`  --inputFileName=`echo $(ls ${data}/*.dat) | sed 's/ /,/g'` --maxFileSize=$((1024*1024*1024)) --outputFileName=out/$i$j$rand.txt --plot=out/$i$j$rand.png
        
    else
        python $loc/File_access_pattern_gen.py --ioRate=$((125*$j)) --numCores=$((10*$j)) --tasksPerCore=1 --execTime=30 --segmentSize=0 --numCycles=1 --readProbability=$readperc --readSize=$(($incinread*16*1024)) --outputSize=$(($incinwrite*16*1024)) --outputPattern=`echo $i`  --inputFileName=`echo $(ls ${data}/*.dat) | sed 's/ /,/g'` --maxFileSize=$((1024*1024*1024)) --outputFileName=out/$i$j$rand.txt --plot=out/$i$j$rand.png 
    fi
    done
done

for i in {1..2};
do
    for j in "${vals[@]}";
    do
    rand=$(( $RANDOM % 2 + 1 ))
    readperc=`seq 0.5 .01 1 | shuf | head -n1`
    incinread=`seq 1 1 50 | shuf | head -n1`
    incinwrite=`seq 1 1 50 | shuf | head -n1`
    echo "The read percentage is equal to $readperc "
    echo "The read modifier is equal to $incinread "
    echo "The write modifier is equal to $incinwrite "

    if [ $i -gt 1 ]
    then
    wr=0
    re=1
    echo "write? $wr"
    echo "read? $re"
    else
    wr=1
    re=0
    echo "write? $wr"
    echo "read? $re"
    fi

    if [ $rand -gt 1 ]
    then
        python $loc/File_access_pattern_gen.py --ioRate=125 --numCores=$((10*$j)) --tasksPerCore=1 --execTime=$((30*$j)) --segmentSize=0 --numCycles=1 --readProbability=$readperc --readSize=$(($incinread*16*1024)) --outputSize=$(($incinwrite*16*1024)) --outputPattern=inter --writeonly=$wr --readonly=$re  --inputFileName=`echo $(ls ${data}/*.dat) | sed 's/ /,/g'` --maxFileSize=$((1024*1024*1024)) --outputFileName=out/$wr$re$j$rand.txt --plot=out/$wr$re$j$rand.png

    else
        python $loc/File_access_pattern_gen.py --ioRate=$((125*$j)) --numCores=$((10*$j)) --tasksPerCore=1 --execTime=30 --segmentSize=0 --numCycles=1 --readProbability=$readperc --readSize=$(($incinread*16*1024)) --outputSize=$(($incinwrite*16*1024)) --outputPattern=inter --writeonly=$wr --readonly=$re  --inputFileName=`echo $(ls ${data}/*.dat) | sed 's/ /,/g'` --maxFileSize=$((1024*1024*1024)) --outputFileName=out/$wr$re$j$rand.txt --plot=out/$wr$re$j$rand.png
    fi
    done
done


for j in "${vals[@]}";
    do
    rand=$(( $RANDOM % 2 + 1 ))
    readperc=`seq 0.5 .01 1 | shuf | head -n1`
    incinread=`seq 1 1 50 | shuf | head -n1`
    incinwrite=`seq 1 1 50 | shuf | head -n1`
    echo "The read percentage is equal to $readperc "
    echo "The read modifier is equal to $incinread "
    echo "The write modifier is equal to $incinwrite "

    if [ $rand -gt 1 ]
    then
        python $loc/File_access_pattern_gen.py --ioRate=125 --numCores=$((10*$j)) --tasksPerCore=1 --execTime=$((30*$j)) --segmentSize=0 --numCycles=1 --readProbability=$readperc --readSize=$(($incinread*16*1024)) --outputSize=$(($incinwrite*16*1024)) --outputPattern=inter  --inputFileName=`echo $(ls ${data}/*.dat) | sed 's/ /,/g'` --maxFileSize=$((1024*1024*1024)) --outputFileName=out/Rand_$j$rand.txt --plot=out/Rand_$j$rand.png --random

    else
        python $loc/File_access_pattern_gen.py --ioRate=$((125*$j)) --numCores=$((10*$j)) --tasksPerCore=1 --execTime=30 --segmentSize=0 --numCycles=1 --readProbability=$readperc --readSize=$(($incinread*16*1024)) --outputSize=$(($incinwrite*16*1024)) --outputPattern=inter  --inputFileName=`echo $(ls ${data}/*.dat) | sed 's/ /,/g'` --maxFileSize=$((1024*1024*1024)) --outputFileName=out/Rand_$j$rand.txt --plot=out/Rand_$j$rand.png --random
    fi
done

for j in "${vals[@]}";
    do
    rand=$(( $RANDOM % 2 + 1 ))
    readperc=`seq 0.5 .01 1 | shuf | head -n1`
    incinread=`seq 1 1 50 | shuf | head -n1`
    incinwrite=`seq 1 1 50 | shuf | head -n1`
    echo "The read percentage is equal to $readperc "
    echo "The read modifier is equal to $incinread "
    echo "The write modifier is equal to $incinwrite "

    if [ $rand -gt 1 ]
    then
        python $loc/File_access_pattern_gen.py --ioRate=125 --numCores=$((10*$j)) --tasksPerCore=1 --execTime=$((30*$j)) --segmentSize=0 --numCycles=1 --readProbability=$readperc --readSize=$(($incinread*16*1024)) --outputSize=$(($incinwrite*16*1024)) --outputPattern=inter  --inputFileName=`echo $(ls ${data}/*.dat) | sed 's/ /,/g'` --maxFileSize=$((1024*1024*1024)) --outputFileName=out/Rand_file_$j$rand.txt --plot=out/Rand_file_$j$rand.png --rr

    else
        python $loc/File_access_pattern_gen.py --ioRate=$((125*$j)) --numCores=$((10*$j)) --tasksPerCore=1 --execTime=30 --segmentSize=0 --numCycles=1 --readProbability=$readperc --readSize=$(($incinread*16*1024)) --outputSize=$(($incinwrite*16*1024)) --outputPattern=inter  --inputFileName=`echo $(ls ${data}/*.dat) | sed 's/ /,/g'` --maxFileSize=$((1024*1024*1024)) --outputFileName=out/Rand_file_$j$rand.txt --plot=out/Rand_file_$j$rand.png --rr
    fi
done
