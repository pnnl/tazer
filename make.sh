#!/bin/bash
module load gcc/8.1.0
mkdir build
cd build
mkdir local_inst
cmake -DCMAKE_INSTALL_PREFIX=local_inst/ ../
make install
cd ../
