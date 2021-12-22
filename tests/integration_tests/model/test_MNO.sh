#!/bin/bash
#SBATCH --time=64:15:00

module load python/3.7.0

for i in {1..20}
do
  for j in {1..20}
  do
    echo "Number: $i"
    python block_sel.py $i $j 
  done
done

#python block_sel.py 
#python Model.py
