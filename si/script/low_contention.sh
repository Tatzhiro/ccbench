cmake -G Ninja -DCMAKE_BUILD_TYPE=RELEASE -DVAL_SIZE=4 -DMASSTREE_USE=0 -DBACK_OFF=0 ..
ninja
for i in 8 16 32 64 96 120 168 224
do 
    numactl --interleave=all ./si.exe -clocks_per_us=2100 -extime=3 -max_ope=16 -rmw=0 -rratio=50 -thread_num=$i -tuple_num=1000000 -ycsb=1 -zipf_skew=0
done