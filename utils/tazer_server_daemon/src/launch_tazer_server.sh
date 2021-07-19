#!/bin/bash
args=("$@")
port=$1
tazer_root=/people/powe445/Projects/update_tazer_file_format/tazer
build_dir=build

module load gcc/8.1.0 python/3.7.0

tazer_server_task_id=`sbatch -A ippd --parsable --exclude=node04,node33,node23,node24,node43 -N1 src/tazer_server.sh ${args[@]}`
while [ -z "$server_node" ]; do
server_node=`squeue -j ${tazer_server_task_id} -h -o "%N"`
done

$tazer_root/$build_dir/test/PingServer $server_node $port 300 1
wait

if [ $? -eq 0 ]
then
    echo "host=${server_node}"
else
    echo "host="
fi
