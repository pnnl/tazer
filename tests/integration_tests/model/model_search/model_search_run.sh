#!/bin/bash
#SBATCH --time=64:15:00

module load python/3.6.6

echo 0 > thread_model.txt

for i in {1..5}
do
  sbatch model_search.sh $i
done

typeset -i cur=$(cat thread_model.txt)
sum=5

while [ $cur -lt $sum ]
do
sleep 1
typeset -i cur=$(cat thread_model.txt)
done

python merge.py
