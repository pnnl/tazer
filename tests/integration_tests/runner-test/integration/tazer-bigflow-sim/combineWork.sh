#!/bin/bash

stopped=0
while true; do
  count=2
  files_1=($1/*)
  length_1=${#files_1[@]}
  while [ $length_1 -eq 0 ]; do
    echo $length_1
    files_1=($1/*)
    length_1=${#files_1[@]}
    sleep 1
  done
  bfstopped=$stopped
  for h in "${files_1[@]}"; do
    files_2=($h/test_*)
    length=${#files_2[@]}
    while [ $length -lt 2 ]; do
      echo $length
      files_2=($h/test_*)
      length=${#files_2[@]}
      sleep 1
    done
    #echo "FFFFFFFFFFFFFFFFFFFFFFFF"
    for (( i=$count; i<${length}; i=i+2 )); do
        FILE=$h/test_$i/access_pattern2.txt
        echo $FILE
        if [ -f "$FILE" ]; then
          FILE2=$h/test_$(( $i-2 ))/access_pattern2.txt
          if [ -f "$FILE2" ]; then
            echo "Found both files :)"
            python combineWork.py $FILE $FILE2 test_work/ac_${i}.txt #$2 
          fi
        fi
        stopped=$(( $stopped+1 ))
    done
    count=$(( $count+1 ))
    count=$(( $count+$bfstopped ))
    done
    #count=$(( $count+$length ))
    if [ $bfstopped != $stopped];then
        bfstopped=$stopped
    fi
done
