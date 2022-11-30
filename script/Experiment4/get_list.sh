# /bin/bash
Num_client=$(($RANDOM%10+10))
echo $Num_client
search_dir=/files0/belo700/speedracer/test/to_remove/tazer/script/paper_experiments/exp4
list_files=("$search_dir"/*)
selFiles=""
for ((i = 0 ; i <= $Num_client ; i++)); do
   selFiles+=${list_files[$i]}
   selFiles+=" "
done
echo $selFiles
