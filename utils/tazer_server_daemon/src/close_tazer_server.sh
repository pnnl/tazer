#!/bin/bash
host=$1
port=$2
tazer_root=/people/powe445/Projects/update_tazer_file_format/tazer
build_dir=build
output_path=$tazer_root/utils/tazer_server_daemon/server_output.txt

module load gcc/8.1.0

$tazer_root/$build_dir/test/CloseServer $host $port

status=$?

cnt=`wc -m < $output_path`
sleep 1
while test `wc -m < $output_path` -ne $cnt ; do
sleep 1
cnt=`wc -m < $output_path`
done
cat $output_path

exit $status