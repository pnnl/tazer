#!/bin/bash

srun -A chess -N1 /people/mutl832/tazer_november/build/src/server/server 6024 > server.log 2>&1
wait
