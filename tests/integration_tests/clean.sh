rm -r runner-test/integration/client/*
rm -r runner-test/integration/server/*
rm -r runner-test/integration/tazer_cache/*
rm -r runner-test/integration/tazer_data/*
#rm -r -f runner-test/integration/tazer-bigflow-sim/test_work/*
rm -r runner-test/integration/tazer-bigflow-sim/slurm-*
rm slurm-*
rm model/slurm-*
echo 0 > model/speed.txt
