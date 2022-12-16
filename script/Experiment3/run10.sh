#!/bin/bash

pystring32=""
pystring64=""
exp_type="D"
for index in  1; do
    for Hb_v in 10; do
        for H_v in 1; do
            for MC_v in 2; do
                for Sth_v in "0.0001"; do
                    for smsize in 32; do
                        Hb_value=${Hb_v} #histogram buckets
                        H_value=${H_v} #0:time, 1:log(time0)
                        MC_value=${MC_v} #0:cost, 1:1, 2:log(cost)
                        Sr_value=1 #0:ratio 1:log ratio
                        Sth_value=${Sth_v} #threshold
            
                        private_size=8 #in MB
                        shared_size=${smsize} #in MB
                        block_size=128 #in KB
                        echo "Running ${index} : ${H_value} ${Hb_value} ${MC_value} ${Sr_value} ${Sth_value} $exp_type $private_size $shared_size $block_size"
                        ./runExp3.sh ${H_value} ${Hb_value} ${MC_value} ${Sr_value} ${Sth_value} $exp_type $private_size $shared_size $block_size
                        echo "Finished ${index}"

                        H_vals=("time" "log")
                        MC_vals=("cost" "1" "log")
                        Sr_vals=("ratio" "log")
                        if [[ "$index" -eq 1 ]]; then
                            if [[ "$smsize" -eq 32 ]]; then
                                pystring32=${pystring32}" "Workload3${exp_type}_Hb-${Hb_value}_H-${H_vals[H_value]}_MC-${MC_vals[MC_value]}_Sr-${Sr_vals[Sr_value]}_Sth-${Sth_value}_SMem-32
                                pystring64=${pystring64}" "Workload3${exp_type}_Hb-${Hb_value}_H-${H_vals[H_value]}_MC-${MC_vals[MC_value]}_Sr-${Sr_vals[Sr_value]}_Sth-${Sth_value}_SMem-64
                            fi
                        fi 
                        mv Results_${exp_type}_0_0_0/ LinearSearch-Baseline/
                        mv Results_${exp_type}_1_0_0/ LinearSearch-Workload3${exp_type}_Hb-${Hb_value}_H-${H_vals[H_value]}_MC-${MC_vals[MC_value]}_Sr-${Sr_vals[Sr_value]}_Sth-${Sth_value}_SMem-${shared_size}_${index}/
                    done
                done
            done
        done
    done
done

echo ${pystring32}

#python average_charts.py Workload3${exp_type}-32MB ${pystring32} Workload3D_Hb-10_H-time_baseline-SMem-32 Workload3D-baseline-baseline-SMem-32
#python average_charts.py Workload3${exp_type}-64MB ${pystring64} Workload3D-Hb-10_H-time_baseline-SMem-64 Workload3D-baseline-baseline-SMem-64