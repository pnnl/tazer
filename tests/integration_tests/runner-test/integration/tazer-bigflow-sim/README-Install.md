-*-Mode: markdown;-*-

$Id$

Building
=============================================================================

BigFlowSim consists of two main components:

1. Workflow Generation (`File_access_pattern_gen.py`)
2. Workflow Playback (`WorkloadSim.cpp`)


To build WorkloadSim.cpp simply type:

```
`make
```


Using
=============================================================================

Creating access patterns
-------------------------
Use File_access_pattern_gen.py to create access pattern files. 

The input arguments can losely be characterized by 4 groups:

1. Experiment/architecture parameters:
   * `--ioRate`: experiment wide io rate (MB/s) -> cumulative i/o rate accross all cores (internally the per core i/o rate == ioRate/numCores)
   * `--tasksPerCore`: number of tasks to execute on each core
   * `--numCores`: number of cores in experiment -> number of simultaneously executing tasks
   * `--execTime`: desired cpu time (seconds)

2. File access parameters:
   * `--numCycles`: number of repetitions through a file
   * `--segmentSize`: size of a segment (0 = filesize)
   * `--numFiles`: number of virtual files to simulate
   * `--random`: produce a random access pattern (can be used in conjuction with the other i/o parameters)


3. Individual I/O parameters:
   * `--readSize`: average size of a read operation
   * `--readCofV`: read size coefficient of variation (normal distribution)
   * `--readProbability`: probability that any fiven read is actually performed 
   * `--outputSize`: total size of data written
   * `--outputPattern`: strided=data written after a "segment" has been read, batch=all data is written at the end

4. Miscellaneous options:
   * `--maxFileSize`: max pyhsical file size if the file we are generating access for 
   * `--inputFileName`: name/path of the file we are generating the accesses for
   * `--outputFileName`: path to save access trace
   * `--verbose`: enable verbose output
   * `--plot`: filename where to save a plot of the resulting access pattern


Acesss file format
------------------------

Each line contains the following 5 elements:
* filename offset size 0 0
* filename: the name/path of the physical file. the simulator will open this file.
* offset: starting offset for this specific i/o operation
* size: size of the i/o operation
* 0 0: currently unused


Executing an access trace
--------------------------
Use the compiled executuable `workloadSim`.

inputs:
  * `--infile`: the file containing the access trace produced by "File_access_pattern_gen.py"
  * `--iorate`: the per task/core iorate. Note: this is the last line printed by File_access_patter_gen.py
  * `--timelimit`: number of seconds to limit the absolute execution time of the simulation ( 0 = run to completion )
  * `--infilesuffix`: append the provided argument to end of inputfile names within the trace
    ex: inputfile from access trace: tazer.dat 
          * `--infilesuffix=meta.in`
        resulting filename = tazer.dat.meta.in


Example:
------------
0. make a dummy input file:

```
dd if=/dev/urandom of=tazer.dat bs=64M count=32
```

1. Sequential Read all bytes in a file: 16K readsize

generate access file:
```
python File_access_pattern_gen.py --ioRate=125 --numCores=10 --tasksPerCore=1 --execTime=30 --segmentSize=0 --numCycles=1 --readProbability=1 --readSize=$((16*1024))  --inputFileName=tazer.dat --maxFileSize=$((1024*1024*1024)) --outputFileName=linear_access_16K.txt --plot=linear_access_16K.png
```

playback access file:
```
./workloadSim --infile linear_access_16K.txt --iorate 12.5
```


2. 8MB segments, 300K readsize, read probabaility .25, 4 cycles

generate access file:

```
python File_access_pattern_gen.py --ioRate=125 --numCores=10 --tasksPerCore=1 --execTime=30 --segmentSize=$((8*1024*1024)) --numCycles=4 --readProbability=.25 --readSize=$((300*1024))  --inputFileName=tazer.dat --maxFileSize=$((1024*1024*1024)) --outputFileName=8MBsegemented_access_4cycles_300K.txt --plot=8MBsegemented_access_4cycles_300K.png
```

playback access file:
```
./workloadSim --infile 8MBsegemented_access_4cycles_300K.txt --iorate 12.5
```


3. 64MB segments, 8k readsize, read probability .25, no cycles, random access, batched output

generate access file:
```
python File_access_pattern_gen.py --ioRate=125 --numCores=10 --tasksPerCore=1 --execTime=30 --segmentSize=$((64*1024*1024)) --readProbability=.25 --readSize=$((8*1024)) --plot=64MBsegemented_random_access_8K.png --inputFileName=tazer.dat --maxFileSize=$((1024*1024*1024)) --outputFileName="64MBsegemented_random_access_8K" -w batched --random
```

playback access file:
```
./workloadSim --infile 64MBsegemented_random_access_8K.txt --iorate 12.5
```

