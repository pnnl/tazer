#!/bin/bash

#change the default folder name and new folder name according to your experiments
for index in 1 2 3 4 5 6 7 8 9 10; do
#for index in 13 14; do
    echo "Running ${index}"
    # cd ../../build/
    # make clean
    # make -j
    # cd ../script/Experiment3/
    ./runExp3.sh
    echo "Finished ${index}"
    mv Results_D_1_1_0/ D-Hb-10_H-log_MC-log_Sr-log_Sth-20_-${index}
done