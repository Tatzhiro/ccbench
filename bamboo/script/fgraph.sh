cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DVAL_SIZE=1000 -DMASSTREE_USE=0 -DBACK_OFF=1 ..
ninja
numactl --interleave=all ./bamboo.exe -clocks_per_us=2100 -extime=10 -max_ope=16 -rmw=0 -rratio=50 -thread_num=32 -tuple_num=100000000 -ycsb=1 -zipf_skew=0.9