#!/usr/bin/perl
use strict;

# runTest parameters
my $port = 6024;
my $fileName = "/qfs/people/firo017/tazer/tazer_2/tazer/build/inputs/warAndPeace.txt";
#my $metaFile = $fileName . ".meta.in";
my $metaFile = "/qfs/people/firo017/tazer/tazer_2/tazer/build/inputs/warAndPeace.txt.meta.in";
my $numThreads = 10;
# Set to zero for linear read through
my $seed = 7;

# workload to run
# my $workloadPath = "/people/suet688/ippd_2021/build/test/CheckCp";
my $workloadPath = "/qfs/people/firo017/tazer/tazer_2/tazer/build/test/CheckCp";
my $workload = "$workloadPath $fileName $numThreads $seed";

# -x node28,node22,node33
my $command = "salloc -w node07,node08  -N 2 ./runTest.pl -c $metaFile -p $port $workload";
print "$command\n";
system($command);
