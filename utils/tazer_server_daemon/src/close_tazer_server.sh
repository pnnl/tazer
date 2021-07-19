#!/bin/bash
host=$1
port=$2
tazer_root=/people/powe445/Projects/update_tazer_file_format/tazer
build_dir=build

module load gcc/8.1.0

$tazer_root/$build_dir/test/CloseServer $host $port

exit $?