#!/bin/bash
#This creates a junit xml file that jenkins uses to report test results.


workspace=$1
cd $workspace
# xml="combined_testing.xml"
# RESULTS_PATH=$workspace/test_results
# echo '<?xml version="1.0" encoding="UTF-8"?>' > $xml
# echo '<testsuites>' >> $xml

failures=0
client_count=0
total_time=0
total_tests=0

for node in `ls -d node*`;
do

    #This parses through the time command output for every client and store the result in seconds in the client_time array.
    #It also looks at the shared_vars.txt files to see how many clients failed.
    for client in `ls  ${node}`;
    do
        client_dir=${node}/${client}
        echo "${client_dir}"
        . ${client_dir}/shared_vars.txt
        failures=$(($failures+$FAILED))
        client_count=$(($client_count+1))

        real_time_output=`grep real* ${client_dir}/${client}.error`
        client_time[$client_count]=0
        i=1
        stop=`grep -c real* ${client_dir}/${client}.error`
        
        while [ $i -le $stop ]
        do
            temp=`echo "$real_time_output" | head -$i | tail -1`
            seconds=`echo "$temp" | cut -d' ' -f 2 | cut -d'm' -f 2 | cut -d's' -f 1`
            minutes=`echo "$temp" | cut -d' ' -f 2 | cut -d'm' -f 1 | cut -d'	' -f 2` 
            minutes=$(echo "$minutes * 60" | bc)
            client_time[$client_count]=$(echo "${client_time[$client_count]} + $seconds + $minutes" | bc)
            i=$(($i+1))
        done
        total_tests=$((total_tests + TESTS_RUN ))
        successes=$(($TESTS_RUN - $FAILED))
        echo "$node $client -- Total tests:  $TESTS_RUN Successes: $successes Failures: $FAILED"
    done
    # echo '<testsuite name="Combined-Testing" errors="0" failures="'$failures'" hostname="tbd" tests="'$client_count'" time="'$total_time'">' >> $xml

     
    # #This loops through every client and makes a test case for each of them. It uses a combination of client#.out client#.error and shared_vars.txt
    # #which are files found in every client# directory
    # i=1
    # for client in `ls  ${node}`;
    # do
    #     client_dir=${node}/${client}
    #     . ${client_dir}/shared_vars.txt

    #     echo '<testcase classname="Combined_Testing.'$TEST_USED'" name="'$node_$client' '$TEST_USED' tests" tests="$TESTS_RUN" time="'${client_time[$i]}'">' >> $xml

    #     if [ $FAILED == 1 ]; then
    #         fail_message=`grep "test failed:" $client_dir/$client.error`
    #         echo '<failure message="'$fail_message'" type="'$TEST_USED'">' >> $xml
    #         echo `grep -A 10 "Error: comparison failed" $client_dir/$client.out` >> $xml
    #         echo '</failure>' >> $xml
    #     fi

    #     echo "<system-out>" >> $xml
    #     echo "$(cat $client_dir/$client.out)" >> $xml
    #     echo "</system-out>" >> $xml

    #     echo "<system-error>" >> $xml
    #     echo "$(cat $client_dir/$client.error)" >> $xml
    #     echo "</system-error>" >> $xml
        
    #     #I couldn't get my custom stat to display in the results. I also tryed adding things to the <testcase>
    #     # my_stat=`grep -c TAZER* $client/$client.out`
    #     # echo "<my-stat>" >> $xml
    #     # echo "My stat is: '$my_stat'" >> $xml
    #     # echo "I want jenkins to report this."
    #     # echo "</my-stat>" >> $xml

    #     echo "</testcase>" >> $xml
    #     i=$(($i+1))
    # done
done

# echo "</testsuite>" >> $xml
# echo "</testsuites>" >> $xml
# mv $xml $RESULTS_PATH

successes=$(($total_tests - $failures))
echo "Total tests: $total_tests Successes: $successes Failures: $failures"
