#!/bin/bash
argcount=$#
args=("$@")
host=`hostname`
port=$1
env_vars=()
tazer_root=/people/powe445/Projects/update_tazer_file_format/tazer
build_dir=build
tazer_server=$tazer_root/$build_dir/src/server/server
workspace=/tmp/tazer${USER}/server
output_path=$tazer_root/utils/tazer_server_daemon/server_output.txt

for ((i=1;i<$argcount;i++)); do 
    env_vars+=${args[${i}]}
    env_vars+=" "
done

echo "Starting tazer server: ${host} ${port}"
echo "env: ${env_vars}"

#clean up from previous runs
rm -r /tmp/*tazer${USER}*
rm -r /state/partition1/*tazer${USER}*
rm /dev/shm/*tazer${USER}*
rm /dev/shm/*tazer*
rm -r /tmp/${USER}/*tazer*

mkdir -p $workspace
cd $workspace

###
# touch tazerWrite.dat
# if [ ! -f tazer1GB.dat ]; then
#     dd if=/dev/urandom of=tazer1GB.dat bs=10M count=100 &
# fi
# wait
###

$env $tazer_server $port $host &> $output_path