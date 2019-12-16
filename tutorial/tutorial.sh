#!/bin/bash

#launch server

#create meta files
python3 ../utils/CreateTazerFiles.py --server="localhost:5001" --path=../src --serverroot=`pwd`/../src





#run "normally" with original files
python3 CountFileSizes.py --path=../src

#run "normally" with tazer files
python3 CountFileSizes.py --path=tazer/src



#clear any old cache files
rm /dev/shm/tazer*
rm -r /tmp/tazer*

#run using tazer -- default is to only use a private inmemory cache that persists only as long as the task
time LD_PRELOAD=../MYINSTALL/lib/libclient.so python3 CountFileSizes.py --path=tazer/src

#rerun using persistant sharedmemory and filesystem caches
time TAZER_SHARED_MEM_CACHE=1 TAZER_SHARED_MEM_CACHE_SIZE=$((32*1024*1024)) \
TAZER_BOUNDED_FILELOCK_CACHE=1 TAZER_BOUNDED_FILELOCK_CACHE_SIZE=$((1*1024*1024*1024)) \
LD_PRELOAD=../MYINSTALL/lib/libclient.so python3 CountFileSizes.py --path=tazer/src

#rerun to exploit the use of the persistant caches
time TAZER_SHARED_MEM_CACHE=1 TAZER_SHARED_MEM_CACHE_SIZE=$((32*1024*1024)) \
TAZER_BOUNDED_FILELOCK_CACHE=1 TAZER_BOUNDED_FILELOCK_CACHE_SIZE=$((1*1024*1024*1024)) \
LD_PRELOAD=../MYINSTALL/lib/libclient.so python3 CountFileSizes.py --path=tazer/src