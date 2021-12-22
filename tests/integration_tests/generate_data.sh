#!/bin/bash

dd if=/dev/urandom of=tazer.dat bs=64M count=32
dd if=tazer.dat of=tazer_small.dat bs=1M count=1 status=none

for (( i=1; i<=$1; i++ ))
do
   rr=$(( $RANDOM % $2 +1 ))
   echo "File $i (size $rr GB)"
   for (( j=1; j<=$rr; j++ ))
   do
      cat tazer.dat >> tazer_$i.dat
   done
done

echo "starting to generate the small files"

for (( i=1; i<=$3; i++ ))
do
   rr=$(( $RANDOM % $4 +1 ))
   echo "File $i (size $rr MB)"
   for (( j=1; j<=$rr; j++ ))
   do
      cat tazer_small.dat >> tazer_small_$i.dat
   done
done
