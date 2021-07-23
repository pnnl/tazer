#!/bin/bash
argcount=$#
args=("$@")
host=`hostname`
port=$1
data_file=$2
data_file_size=$3
env_vars_count=$4
metafile=${args[$((4 + $env_vars_count))]}
metafile_lines_count=${args[$((5 + $env_vars_count))]}
env_vars=()
metafile_lines=()
tazer_root=/people/powe445/Projects/update_tazer_file_format/tazer
build_dir=build
tazer_server=$tazer_root/$build_dir/src/server/server
workspace=/tmp/tazer${USER}/server
output_path=$tazer_root/utils/tazer_server_daemon/server_output.txt

for ((i=4;i<$((4 + $env_vars_count));i++)); do 
    env_vars+=${args[${i}]}
    env_vars+=" "
done

for ((i=$((6 + $env_vars_count));i<$((6 + $env_vars_count + $metafile_lines_count));i++)); do 
    metafile_lines+=${args[${i}]}
    metafile_lines+=" "
done

echo "Starting tazer server: ${host} ${port}"
echo "env: ${env_vars}"

#clean up from previous runs
rm -r /tmp/*tazer${USER}*
rm -r /state/partition1/*tazer${USER}*
rm /dev/shm/*tazer${USER}*
rm /dev/shm/*tazer*
rm -r /tmp/${USER}/*tazer*

rm -r $workspace
mkdir -p $workspace
cd $workspace

if [ $data_file != 0 ];
then
    echo "creating file: ${data_file}, size: ${data_file_size} MB"
    dd if=/dev/urandom of=$data_file bs=1M count=$data_file_size &
    wait
fi

if [ $metafile != 0 ];
then
    echo "creating Tazer metafile: ${metafile}"
    touch $metafile
    for line in $metafile_lines; do
        echo $line >> $metafile
    done
fi

$env $tazer_server $port $host &> $output_path