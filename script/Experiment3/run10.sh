#!/bin/bash

#change the default folder name and new folder name according to your experiments
for index in 1 2 3 4 5 6 7 8 9 10; do
    echo "Running ${index}"
    ./runExp3.sh
    echo "Finished ${index}"
    mv Results_E_1_1_0/ Results_E_1_1_0_umb-base-files0-${index}
done