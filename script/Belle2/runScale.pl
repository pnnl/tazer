#!/usr/bin/perl
use strict;

# runTest parameters
my $numNodes = 2;
my $thread = 1;
my $port = 6024;
my $metaFile = "BHWide_ECL-phase3-optimized.root.meta.in,Coulomb_HER_ECL-phase3-optimized.root.meta.in,RBB_ECL-phase3-optimized.root.meta.in,Touschek_LER_ECL-phase3-optimized.root.meta.in,BHWideLargeAngle_ECL-phase3-optimized.root.meta.in,Coulomb_HER_PXD-phase3-optimized.root.meta.in,RBB_PXD-phase3-optimized.root.meta.in,Touschek_LER_PXD-phase3-optimized.root.meta.in,BHWideLargeAngle_PXD-phase3-optimized.root.meta.in,Coulomb_HER_usual-phase3-optimized.root.meta.in,RBB_usual-phase3-optimized.root.meta.in,Touschek_LER_usual-phase3-optimized.root.meta.in,BHWideLargeAngle_usual-phase3-optimized.root.meta.in,Coulomb_LER_ECL-phase3-optimized.root.meta.in,Touschek_HER_ECL-phase3-optimized.root.meta.in,twoPhoton_ECL-phase3-optimized.root.meta.in,BHWide_PXD-phase3-optimized.root.meta.in,Coulomb_LER_PXD-phase3-optimized.root.meta.in,Touschek_HER_PXD-phase3-optimized.root.meta.in,twoPhoton_PXD-phase3-optimized.root.meta.in,BHWide_usual-phase3-optimized.root.meta.in,Coulomb_LER_usual-phase3-optimized.root.meta.in,Touschek_HER_usual-phase3-optimized.root.meta.in,twoPhoton_usual-phase3-optimized.root.meta.in";

# BigFlowSim parameters
#my $dataFile = "Test2B_random.txt";
my $dataFile = shift @ARGV;
if (! -e $dataFile) {
    print "\"$dataFile\" is not a file. Please enter an input file.\n";
    exit 1;
}

my $ioIntensity = 10.0;
my $kernel = 0;

# workload to run
my $workloadPath = "/people/mutl832/tazer-bigflowsim/workloadSim";
my $workload = "$workloadPath -f $dataFile -i $ioIntensity -m .meta.in -k $kernel";
# my $workload = "$workloadPath -f $dataFile -i $ioIntensity -k $kernel";
my @allocators = (0); #, 1, 2, 3, 4, 5);
foreach my $allocator (@allocators) {
    my $command = "salloc -A ippd -x node28,node22,node42,node40,node33,node07,node25,node38  -N $numNodes ./runTest.pl -c $metaFile -p $port -a $allocator $workload";
    print "$command\n";
    system($command);
}