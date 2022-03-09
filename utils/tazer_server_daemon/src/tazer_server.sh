#!/bin/bash
argcount=$#
args=("$@")
host=`hostname`
port=$1
# data_file=$2
# data_file_size=$3
# env_vars_count=$4
# metafile=${args[$((4 + $env_vars_count))]}
# metafile_lines_count=${args[$((5 + $env_vars_count))]}
env_vars=()
# metafile_lines=()
tazer_root=/people/${USER}/Projects/tazer
build_dir=build_sp
tazer_server=$tazer_root/$build_dir/src/server/server
output_path=$tazer_root/utils/tazer_server_daemon/output
workspace=${HOME}/tazer/belle2_data/

rm -r $output_path
mkdir -p ${output_path}
rm server.log

N=1
expName="tazer_server"
taskType="tazer"
ioType="tazer"

t=$(date +%s)
var_names="StartTime" && var_vals="${t}" && var_times="${t}"
var_names="${var_names},N" && var_vals="${var_vals},${N}" && var_times="${var_times},${t}"
var_names="${var_names},ExpName" && var_vals="${var_vals},${expName}" && var_times="${var_times},${t}"
var_names="${var_names},TaskType" && var_vals="${var_vals},${taskType}" && var_times="${var_times},${t}"
var_names="${var_names},Host" && var_vals="${var_vals},${host}" && var_times="${var_times},${t}"
var_names="${var_names},IOType" && var_vals="${var_vals},${ioType}" && var_times="${var_times},${t}"
var_names="${var_names},Slot" && var_vals="${var_vals},${1}" && var_times="${var_times},${t}"

t=$(date +%s)
var_names="${var_names},StartSetup" && var_vals="${var_vals},${t}" && var_times="${var_times},${t}"
t=$(date +%s)
var_names="${var_names},StartInputTx" && var_vals="${var_vals},${t}" && var_times="${var_times},${t}"
t=$(date +%s)
var_names="${var_names},StopInputTx" && var_vals="${var_vals},${t}" && var_times="${var_times},${t}"
for ((i=4;i<$((4 + $env_vars_count));i++)); do 
    env_vars+=${args[${i}]}
    env_vars+=" "
done

# for ((i=$((6 + $env_vars_count));i<$((6 + $env_vars_count + $metafile_lines_count));i++)); do 
#     metafile_lines+=${args[${i}]}
#     metafile_lines+=" "
# done

echo "Starting tazer server: ${host} ${port}"
echo "env: ${env_vars}"

#clean up from previous runs
rm -r /tmp/*tazer${USER}*
# rm -r /state/partition1/*tazer${USER}*
rm /dev/shm/*tazer${USER}*
rm /dev/shm/*tazer*
rm -r /tmp/${USER}/*tazer*


mkdir -p $workspace
cd $workspace

# if [ $data_file != 0 ];
# then
#     echo "creating file: ${data_file}, size: ${data_file_size} MB"
#     dd if=/dev/urandom of=$data_file bs=1M count=$data_file_size &
#     wait
# fi

# if [ $metafile != 0 ];
# then
#     echo "creating Tazer metafile: ${metafile}"
#     touch $metafile
#     for line in $metafile_lines; do
#         echo $line >> $metafile
#     done
# fi

t=$(date +%s)
var_names="${var_names},StartExp" && var_vals="${var_vals},${t}" && var_times="${var_times},${t}"
export TAZER_THREAD_STATS=1
$env $tazer_server $port  &> ${output_path}/server.log

t=$(date +%s)
var_names="${var_names},StopExp" && var_vals="${var_vals},${t}" && var_times="${var_times},${t}"
parsed=`python ${tazer_root}/utils/plot_thread_times/ParseTazerOutput.py -s ${output_path}/server.log`
echo "$parsed"
tmp_names=`echo "$parsed" | grep -oP '(?<=labels:).*'`
tmp_vals=`echo "$parsed" | grep -oP '(?<=vals:).*'`
var_names="${var_names},${tmp_names}" && var_vals="${var_vals},${tmp_vals}"
var_names="${var_names},FinishedTime" && var_vals="${var_vals},${t}" && var_times="${var_times},${t}"
echo "0;$var_names;$var_vals" > $output_path/server_output.txt