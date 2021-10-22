#!/usr/bin/perl
use strict;

# runTest parameters
my $port = 6024;
my $fileName = "tazer.dat";
my $metaFile = $fileName . ".meta.in";
my $numThreads = 10;
# Set to zero for linear read through
my $seed = 7;

# workload to run
my $workloadPath = "/people/suet688/ippd_2021/build/test/CheckCp";
my $workload = "$workloadPath $fileName $numThreads $seed";


my $command = "salloc -x node28,node22 -N 2 ./runTest.pl -c $metaFile -p $port $workload";
print "$command\n";
system($command);
