#!/usr/bin/perl
use strict;
use Socket;
use Getopt::Std;

my $jobId = $ENV{'SLURM_JOB_ID'};

my %options=();
getopts("c:x:p:a:d", \%options);
my @clientFiles = defined $options{c} ? split(',', $options{c}) : "";
my $procsPerNode = defined $options{x} ? $options{x} : 1;
my $port = defined $options{p} ? $options{p} : 6024;
my $dontRun = defined $options{d} ? 1 : 0;
my $allocator = defined $options{a} ? $options{a} : -1;
my $maxDigit = 10;

my $path = "/people/mutl832/tazer-merged/build";
my $serverPath = $path . "/src/server/server";
my $libPath = $path . "/src/client/libclient.so";
my $closeServerPath = $path . "/test/CloseServer";
my $pingServerPath = $path . "/test/PingServer";

my $preloadLib  = "env LD_PRELOAD=$libPath";
my $command = "echo NOOP";
if(@ARGV)
{
    $command = $preloadLib . " " . join(" ",@ARGV);
}
print "$command\n";

if($allocator > -1) {
    $ENV{'TAZER_SCALABLE_CACHE_ALLOCATOR'} = $allocator;
}

my $nodeString = $ENV{'SLURM_JOB_NODELIST'};
my @nodeList = split('],', $nodeString);
my @nodes;
foreach my $entry (@nodeList)
{
    if($entry =~ m/\[/)
    {
        my @internal1 = split('\[', $entry);
        my $prefix = $internal1[0];
        my @internal2 = split(',', $internal1[1]);
        foreach my $temp (@internal2)
        {
            $temp =~ s/\]//;
            if($temp =~ m/-/)
            {
                my @internal3 = split('-', $temp);
                for(my $i=$internal3[0]; $i<=$internal3[1]; $i++)
                {
                    $i+=0; #remove leading zeros
                    my $zeros = "";
                    for(my $j = $maxDigit; $j > 0; $j/=10)
                    {
                        if($i < $j)
                        {
                            $zeros = $zeros . "0";
                        }
                    }
                    push(@nodes, $prefix . $zeros . $i);
                }
            }
            else
            {
                push(@nodes, $prefix . $temp);
            }
        }
    }
    else
    {
        push(@nodes, $entry);
    }
}

if($#nodes >= 1)
{
    my $server = shift(@nodes);
#        my $serverIp = inet_ntoa(inet_aton($server));
    my $serverIp = $server;
    my @clients = @nodes;
    
    foreach my $file (@clientFiles) {
        if($file =~ /meta/) {
            my $newText = "";
            open  my $fh, "<", $file or die "Could not open $file: $!";

            while ( my $line = <$fh> ) {
                if (index( $line, 'host' ) > -1 ){
                    $newText .= "host=$serverIp\n";
                }
                else {
                    $newText .= $line; 
                }
            }   
            close $fh;

            open(my $fh2, '>', $file) or die "Could not open file '$file' $!";
            print $fh2 "$newText";
            close $fh2;
            print "$file\n";
        }
    }

    if(!$dontRun)
    {
        my $serverCommand = "srun -N 1 -w $server $serverPath $port &";
        print "$serverCommand\n";
        system($serverCommand);

        my $pingCommand = "srun -N 1 -w $server $pingServerPath $server $port";
        print "$pingCommand\n";
        print `$pingCommand`;


        foreach my $clientNode (@clients)
        {
            my $command = "srun -N 1 -n $procsPerNode -w $clientNode $command";
            print "$command\n";
            print `$command`;
                
        }

        my $shutdownCommand = "srun -N 1 -w $server $closeServerPath $server $port";
        print "$shutdownCommand\n";
        print `$shutdownCommand`;
    }
}

