cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DVAL_SIZE=4 -DMASSTREE_USE=0 -DBACK_OFF=1 ..
ninja
for i in 56 112 168 224
do 
    numactl --interleave=all ./silo.exe -clocks_per_us=2100 -extime=3 -max_ope=16 -rmw=0 -rratio=50 -thread_num=$i -tuple_num=1000000 -ycsb=1 -zipf_skew=0.99
done
# numactl --interleave=all ./bamboo.exe -clocks_per_us=2100 -extime=3 -max_ope=16 -rmw=0 -rratio=50 -thread_num=16 -tuple_num=100000000 -ycsb=1 -zipf_skew=0.99
