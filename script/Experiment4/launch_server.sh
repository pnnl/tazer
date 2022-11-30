#!/bin/bash

srun -A oddite -N1 /files0/belo700/speedracer/test/to_remove/tazer/build/src/server/server 6024 > server.log 2>&1
wait
