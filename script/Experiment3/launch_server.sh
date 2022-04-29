#!/bin/bash

srun -A ippd -N1 /people/mutl832/tazer-paper/build/src/server/server 6024 > server.log 2>&1
wait