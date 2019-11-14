#!/usr/bin/perl
use strict;
use Socket;

my $maxDigit = 10;

my $serverPath = "@SERVER_PATH@";
my $libPath = "@LIB_PATH@";
my $closeServerPath = "@CLOSE_SERVER_PATH@";
my $pingServerPath = "@PING_SERVER_PATH@";

#if(@ARGV)
#{
    my $jobToRun = shift(@ARGV);
    my @clientFiles = @ARGV;

    my $jobId = $ENV{'SLURM_JOB_ID'};
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
        
        my @fileArgs;
        foreach my $file (@clientFiles) {
            if($file =~ /meta/) {
                open  my $fh, "<", $file or die "Could not open $file: $!";
                my $text = <$fh>;
                close $fh;

                my @parts = split(':', $text);
                $parts[0] = $serverIp;
                my $newText = join(':', @parts);

                my @parts = split('/', $file);
                $parts[$#parts] = $jobId . "_" . $parts[$#parts];
                my $newFileName = join('/', @parts);
                print "$newFileName\n";

                open(my $fh2, '>', $newFileName) or die "Could not open file '$newFileName' $!";
                print $fh2 "$newText";
                close $fh2;
                push(@fileArgs, $newFileName);
            }
            else {
                push(@fileArgs, $file);
            }
        }

        my $serverCommand = "srun -N 1 -w $server $serverPath &";
        print "$serverCommand\n";
        system($serverCommand);

        my $pingCommand = "srun -N 1 -w $server $pingServerPath $server";
        print "$pingCommand\n";
        print `$pingCommand`;

        if(defined $jobToRun) 
        {
            foreach my $clientNode (@clients)
            {
                my $command = "srun -N 1 -w $clientNode env LD_PRELOAD=$libPath $jobToRun @fileArgs";
                print "$command\n";
                print `$command`;
                    
            }
        }

        my $shutdownCommand = "srun -N 1 -w $server $closeServerPath $server";
        print "$shutdownCommand\n";
        print `$shutdownCommand`;

        # foreach my $deleteFile (@fileArgs)
        # {
        #     if($deleteFile =~ /meta/) {
        #         my $deleteCommand = "rm $deleteFile";
        #         print "$deleteCommand\n";
        #         `$deleteCommand`;
        #     }
        # }
    }
#}