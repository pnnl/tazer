#!/bin/bash
#SBATCH --time=64:15:00

module load python/3.6.6

python model_search.py $1
