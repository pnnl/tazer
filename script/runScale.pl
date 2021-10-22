#!/usr/bin/perl
use strict;

# runTest parameters
my $numNodes = 2;
my $thread = 1;
my $port = 6024;
my $metaFile = "tazer.dat.meta.in,tazer2.dat.meta.in,tazer3.dat.meta.in,tazer4.dat.meta.in,tazer5.dat.meta.in";

# BigFlowSim parameters
# my $dataFile = "linear_access_8K.txt";
# my $dataFile = "stride_8kS_1kR_1C_2.txt";
# my $dataFile = "small.txt"; 
# my $dataFile = "small_linear.txt";
# my $dataFile = "small_random.txt";
# my $dataFile = "64MB_random_access.txt";
# my $dataFile = "64MB_linear_access.txt";
my $dataFile = "exp2.txt";
# my $dataFile = "exp3.txt";
# my $dataFile = "16Test.txt";
# my $dataFile = "smallSomeAccess.txt";
# my $dataFile = "exp4.txt";
# my $dataFile = "exp5.txt";
# my $dataFile = "exp6.txt";
# my $dataFile = "exp7.txt";
# my $dataFile = "exp8.txt";
# my $dataFile = "exp9.txt";
# my $dataFile = "exp10.txt";
my $ioIntensity = 10.0;
my $kernel = 0;

# workload to run
my $workloadPath = "/people/suet688/bigFlowSim/tazer-bigflow-sim/build/WorkloadSim";
my $workload = "$workloadPath -f $dataFile -i $ioIntensity -m .meta.in -k $kernel";
# my $workload = "$workloadPath -f $dataFile -i $ioIntensity -k $kernel";
my @allocators = (0); #, 1, 2, 3, 4, 5);
foreach my $allocator (@allocators) {
    my $command = "salloc -x node28,node22,node42 -N $numNodes ./runTest.pl -c $metaFile -p $port -a $allocator $workload";
    print "$command\n";
    system($command);
}

# Check with master
# Single task staging both random and linear

# Both open --> not victim --> check behavoir --> other noraml...
# One closed -- one at a time --> victim cache --> Belle II
# Check one at a time maybe good for Belle II

# Check how the blocksizes are set --> 64MB cache size, 1MB Blocksize, 8K reads
# Random will be terrible!

# Reads = blocksize to remove prefetching effects

# Run both regular tazer and scalable tazer

# File size = cache size = S
# -- Test 1a: Cycle one random file of S only 4-5x (access each block 4-5 times)
# -- Test 1b: Linear
# -- Test 2: Two files of S, one streaming, one random, 4-5x
# -- Test 3: Read once, two files, streaming cycle 1x, random cycle 4x

# -- Compare against LRU default cache

# Aggregate stat is good!

# July 13
# Algorithm vs Implementation
# Check trace block -- print what block is brought in vs evicted

# for growth rates interleave reads in the correct ratio

# ------------------------------------------------------

# Strided --> 1 blocks
# Otherwise Available / numFiles

# Random for regualar and scalable should be same

# Corner case to check
# Read that spans multiple blocks

# ------------------------------------------------------
# allocation processes:
# 1. Are there any free
# 2. Are there any left in the closed files (victims)
# 3. Are we need to take from some open files

# Update rank -> marginal unit benifit (eqn 2) for the stealing

# Note: not considering a file --> we are interested in data objects (as unique)

# Note: look at unbounded and cache.h and compare to bounded

# ------------------------------------------------------
# Sept 17
# Add evictions to cache stats

# Go over stats in paper 

# tazer.dat 18740 10052 0 0
# tazer.dat 1048576 9805 0 0
# tazer.dat 1058381 8227 0 0
# tazer.dat 1066608 7975 0 0
# tazer.dat 2097152 8086 0 0
# tazer.dat 2105238 9574 0 0
# tazer.dat 2114812 8743 0 0
# tazer.dat 3145728 9517 0 0

# ------------------------------------------------------
# We are using a real histogram
# The buckets are the MISS INTERVAL (we are NOT moving linearly along the x-axis)
# See the footprint growth graph
# Sp is the size of the partition

# ------------------------------------------------------
# Get test from Ocean
# Track evictions (capacity misses not compulsery)
# Eviction counter doesn't count streaming evictions

# 1 Streaming + several segment random (different sizes) + different reps (read times) -- all compared to size

# ------------------------------------------------------
# Single Task test
# Cache size of S=64 Block 1MB 
# s = 8MB -> block stride
# File Size = n * s -> n = 10 -> s 10, 20, 40, ... 
# Segment size is factors of blocks 1MB 2MB 4MB
# Read size is 8K
# Probability is 1
# All files are the same size
# Do 4x

# Multi Task test
# --------------------
#
# Code review
# Run exp 1a, 1b, 2, 3
# 