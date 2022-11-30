#!/bin/bash
#SBATCH --time=64:15:00

pystring32=""
pystring64=""
TestCase=(20) #(10 15 20)
SharedSize=(8 8 8)
#exp_type="E"
for i in ${!TestCase[@]}; do 
exp_type=${TestCase[$i]}
for index in 6 7 8 9 10; do #1 2 3 4 5 6 7 8 9 10; do
    for Hb_v in 100; do #10 50; do #10 50; do
        for H_v in 1; do # 0; do
            for MC_v in 0; do # 2; do
                for Sth_v in 50; do #10 50; do
                    for smsize in ${SharedSize[$i]}; do
                        Hb_value=${Hb_v} #histogram buckets
                        H_value=${H_v} #0:time, 1:log(time0)
                        MC_value=${MC_v} #0:cost, 1:1, 2:log(cost)
                        Sr_value=1 #0:ratio 1:log ratio
                        Sth_value=${Sth_v} #threshold
            
                        private_size=4 #in MB
                        shared_size=${smsize} #in MB
                        block_size=64 #in KB
                        echo "Running ${index} : ${H_value} ${Hb_value} ${MC_value} ${Sr_value} ${Sth_value} $exp_type $private_size $shared_size $block_size"
                        ./Experiment4/runExp3.sh ${H_value} ${Hb_value} ${MC_value} ${Sr_value} ${Sth_value} $exp_type $private_size $shared_size $block_size
                        echo "Finished ${index}"

                        H_vals=("time" "log")
                        MC_vals=("cost" "1" "log")
                        Sr_vals=("ratio" "log")
                        mv Experiment4/Results_${exp_type}_0_1_0/ Experiment4/LinearSearch-Workload4${exp_type}_Baseline-fullAssociativity_${index}/
                        mv Experiment4/Results_${exp_type}_1_1_0/ Experiment4/LinearSearch-Workload4${exp_type}_Hb-${Hb_value}_H-${H_vals[H_value]}_MC-${MC_vals[MC_value]}_Sr-${Sr_vals[Sr_value]}_Sth-${Sth_value}_UMBTH-0.005_SMem-${shared_size}_UpdatedSHared-Burcu_${index}/
                    done
                done
            done
        done
    done
done
done

echo ${pystring32}

#python average_charts.py Workload3${exp_type}-32MB ${pystring32} Workload3D_Hb-10_H-time_baseline-SMem-32 Workload3D-baseline-baseline-SMem-32
#python average_charts.py Workload3${exp_type}-64MB ${pystring64} Workload3D-Hb-10_H-time_baseline-SMem-64 Workload3D-baseline-baseline-SMem-64
