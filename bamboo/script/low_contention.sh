cmake -G Ninja -DCMAKE_BUILD_TYPE=RELWITHDEBINFO -DVAL_SIZE=4 -DMASSTREE_USE=0 -DBACK_OFF=0 -DNONTS=0 -DFAIR=0 -DRANDOM=1 ..
ninja
for i in 28 56 84 112 140 168 196 224
do 
    numactl --interleave=all ./bamboo.exe -clocks_per_us=2100 -extime=3 -max_ope=16 -rmw=0 -rratio=50 -thread_num=$i -tuple_num=1000000 -ycsb=1 -zipf_skew=0
done
# numactl --interleave=all ./bamboo.exe -clocks_per_us=2100 -extime=3 -max_ope=16 -rmw=0 -rratio=50 -thread_num=16 -tuple_num=100000000 -ycsb=1 -zipf_skew=0
