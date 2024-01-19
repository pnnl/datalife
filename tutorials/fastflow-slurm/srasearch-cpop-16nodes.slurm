#!/bin/bash
#SBATCH --job-name=srasearch16
#SBATCH --partition=slurm
#SBATCH --account=$SLURM_PROJECT
#SBATCH --time=03:50:00
#SBATCH -N 16

$(bash nodenames.sh)
for i in $(seq 0 15)
do
    htime=$(date +%s%3N)
    nodename=node$i
    n=${!nodename}
    rtime=$(ssh $n date +%s%3N)
    echo node$i,$(($htime - $rtime)),$htime,$rtime >> time_diff.log
done

module load perl
stage_start=$(date +%s%3N)
export ID0000001=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2-build_ID0000001
export ID0000001_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2-build_ID0000001.sh &
wait
duration=$(( $(date +%s%3N) - $stage_start ))
echo bowtie2-build, $duration >> stage_stat.log
stage_start=$(date +%s%3N)
export ID0000002=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000002
export ID0000002_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000002.sh &
export ID0000004=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000004
export ID0000004_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000004.sh &
export ID0000006=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000006
export ID0000006_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000006.sh &
export ID0000008=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000008
export ID0000008_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000008.sh &
export ID0000010=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000010
export ID0000010_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000010.sh &
export ID0000012=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000012
export ID0000012_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000012.sh &
export ID0000014=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000014
export ID0000014_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000014.sh &
export ID0000016=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000016
export ID0000016_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000016.sh &
export ID0000018=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000018
export ID0000018_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000018.sh &
export ID0000020=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000020
export ID0000020_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000020.sh &
export ID0000022=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000022
export ID0000022_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000022.sh &
export ID0000024=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000024
export ID0000024_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000024.sh &
export ID0000026=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000026
export ID0000026_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000026.sh &
export ID0000028=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000028
export ID0000028_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000028.sh &
export ID0000030=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000030
export ID0000030_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000030.sh &
export ID0000032=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000032
export ID0000032_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000032.sh &
export ID0000034=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000034
export ID0000034_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000034.sh &
export ID0000036=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000036
export ID0000036_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000036.sh &
export ID0000038=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000038
export ID0000038_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000038.sh &
export ID0000040=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000040
export ID0000040_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000040.sh &
export ID0000042=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000042
export ID0000042_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000042.sh &
export ID0000044=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000044
export ID0000044_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000044.sh &
export ID0000046=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000046
export ID0000046_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000046.sh &
export ID0000048=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000048
export ID0000048_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000048.sh &
export ID0000050=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000050
export ID0000050_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000050.sh &
export ID0000052=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000052
export ID0000052_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000052.sh &
export ID0000054=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000054
export ID0000054_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000054.sh &
export ID0000056=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000056
export ID0000056_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000056.sh &
export ID0000058=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000058
export ID0000058_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000058.sh &
export ID0000060=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000060
export ID0000060_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000060.sh &
export ID0000062=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000062
export ID0000062_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000062.sh &
export ID0000064=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000064
export ID0000064_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000064.sh &
export ID0000066=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000066
export ID0000066_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000066.sh &
export ID0000068=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000068
export ID0000068_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000068.sh &
export ID0000070=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000070
export ID0000070_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000070.sh &
export ID0000072=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000072
export ID0000072_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000072.sh &
export ID0000074=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000074
export ID0000074_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000074.sh &
export ID0000076=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000076
export ID0000076_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000076.sh &
export ID0000078=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000078
export ID0000078_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000078.sh &
export ID0000080=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000080
export ID0000080_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000080.sh &
export ID0000082=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000082
export ID0000082_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000082.sh &
export ID0000084=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000084
export ID0000084_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000084.sh &
export ID0000086=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000086
export ID0000086_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000086.sh &
export ID0000088=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000088
export ID0000088_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000088.sh &
export ID0000090=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000090
export ID0000090_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000090.sh &
export ID0000092=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000092
export ID0000092_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000092.sh &
export ID0000094=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000094
export ID0000094_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000094.sh &
export ID0000096=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000096
export ID0000096_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000096.sh &
export ID0000098=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000098
export ID0000098_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000098.sh &
export ID0000100=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000100
export ID0000100_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000100.sh &
export ID0000102=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000102
export ID0000102_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000102.sh &
export ID0000104=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000104
export ID0000104_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000104.sh &
export ID0000106=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000106
export ID0000106_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000106.sh &
export ID0000108=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000108
export ID0000108_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000108.sh &
export ID0000110=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000110
export ID0000110_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000110.sh &
export ID0000112=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000112
export ID0000112_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000112.sh &
export ID0000114=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000114
export ID0000114_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000114.sh &
export ID0000116=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000116
export ID0000116_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000116.sh &
export ID0000118=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000118
export ID0000118_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000118.sh &
export ID0000120=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000120
export ID0000120_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000120.sh &
export ID0000122=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000122
export ID0000122_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000122.sh &
export ID0000124=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000124
export ID0000124_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000124.sh &
export ID0000126=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000126
export ID0000126_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000126.sh &
export ID0000128=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000128
export ID0000128_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000128.sh &
export ID0000130=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000130
export ID0000130_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000130.sh &
export ID0000132=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000132
export ID0000132_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000132.sh &
export ID0000134=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000134
export ID0000134_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000134.sh &
export ID0000136=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000136
export ID0000136_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000136.sh &
export ID0000138=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000138
export ID0000138_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000138.sh &
export ID0000140=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000140
export ID0000140_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000140.sh &
export ID0000142=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000142
export ID0000142_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000142.sh &
export ID0000144=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000144
export ID0000144_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000144.sh &
export ID0000146=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000146
export ID0000146_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000146.sh &
export ID0000148=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000148
export ID0000148_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000148.sh &
export ID0000150=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000150
export ID0000150_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000150.sh &
export ID0000152=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000152
export ID0000152_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000152.sh &
export ID0000154=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000154
export ID0000154_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000154.sh &
export ID0000156=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000156
export ID0000156_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000156.sh &
export ID0000158=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000158
export ID0000158_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000158.sh &
export ID0000160=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000160
export ID0000160_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000160.sh &
export ID0000162=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000162
export ID0000162_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000162.sh &
export ID0000164=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000164
export ID0000164_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000164.sh &
export ID0000166=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000166
export ID0000166_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000166.sh &
export ID0000168=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000168
export ID0000168_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000168.sh &
export ID0000170=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000170
export ID0000170_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000170.sh &
export ID0000172=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000172
export ID0000172_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000172.sh &
export ID0000174=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000174
export ID0000174_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000174.sh &
export ID0000176=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000176
export ID0000176_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000176.sh &
export ID0000178=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000178
export ID0000178_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000178.sh &
export ID0000180=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000180
export ID0000180_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000180.sh &
export ID0000182=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000182
export ID0000182_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000182.sh &
export ID0000184=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000184
export ID0000184_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000184.sh &
export ID0000186=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000186
export ID0000186_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000186.sh &
export ID0000188=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000188
export ID0000188_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000188.sh &
export ID0000190=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000190
export ID0000190_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000190.sh &
export ID0000192=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000192
export ID0000192_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000192.sh &
export ID0000194=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000194
export ID0000194_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000194.sh &
export ID0000196=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000196
export ID0000196_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000196.sh &
export ID0000198=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000198
export ID0000198_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000198.sh &
export ID0000200=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000200
export ID0000200_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000200.sh &
export ID0000202=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000202
export ID0000202_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000202.sh &
export ID0000204=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000204
export ID0000204_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000204.sh &
export ID0000206=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000206
export ID0000206_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000206.sh &
export ID0000208=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000208
export ID0000208_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000208.sh &
export ID0000210=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000210
export ID0000210_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000210.sh &
export ID0000212=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000212
export ID0000212_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000212.sh &
export ID0000214=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000214
export ID0000214_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000214.sh &
export ID0000216=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000216
export ID0000216_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000216.sh &
export ID0000218=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000218
export ID0000218_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000218.sh &
export ID0000220=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000220
export ID0000220_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000220.sh &
export ID0000222=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000222
export ID0000222_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000222.sh &
export ID0000224=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000224
export ID0000224_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000224.sh &
export ID0000226=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000226
export ID0000226_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000226.sh &
export ID0000228=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000228
export ID0000228_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000228.sh &
export ID0000230=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000230
export ID0000230_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000230.sh &
export ID0000232=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000232
export ID0000232_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000232.sh &
export ID0000234=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000234
export ID0000234_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000234.sh &
export ID0000236=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000236
export ID0000236_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000236.sh &
export ID0000238=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000238
export ID0000238_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000238.sh &
export ID0000240=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000240
export ID0000240_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000240.sh &
export ID0000242=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000242
export ID0000242_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000242.sh &
export ID0000244=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000244
export ID0000244_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000244.sh &
export ID0000246=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000246
export ID0000246_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000246.sh &
export ID0000248=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000248
export ID0000248_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000248.sh &
export ID0000250=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000250
export ID0000250_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000250.sh &
export ID0000252=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000252
export ID0000252_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000252.sh &
export ID0000254=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000254
export ID0000254_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000254.sh &
export ID0000256=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000256
export ID0000256_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000256.sh &
export ID0000258=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000258
export ID0000258_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000258.sh &
export ID0000260=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000260
export ID0000260_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000260.sh &
export ID0000262=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000262
export ID0000262_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000262.sh &
export ID0000264=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000264
export ID0000264_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000264.sh &
export ID0000266=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000266
export ID0000266_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000266.sh &
export ID0000268=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000268
export ID0000268_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000268.sh &
export ID0000270=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000270
export ID0000270_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000270.sh &
export ID0000272=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000272
export ID0000272_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000272.sh &
export ID0000274=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000274
export ID0000274_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000274.sh &
export ID0000276=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000276
export ID0000276_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000276.sh &
export ID0000278=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000278
export ID0000278_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000278.sh &
export ID0000280=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000280
export ID0000280_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000280.sh &
export ID0000282=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000282
export ID0000282_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000282.sh &
export ID0000284=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000284
export ID0000284_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000284.sh &
export ID0000286=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000286
export ID0000286_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000286.sh &
export ID0000288=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000288
export ID0000288_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000288.sh &
export ID0000290=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000290
export ID0000290_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000290.sh &
export ID0000292=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000292
export ID0000292_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000292.sh &
export ID0000294=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000294
export ID0000294_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000294.sh &
export ID0000296=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000296
export ID0000296_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000296.sh &
export ID0000298=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000298
export ID0000298_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000298.sh &
export ID0000300=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000300
export ID0000300_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000300.sh &
export ID0000302=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000302
export ID0000302_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000302.sh &
export ID0000304=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000304
export ID0000304_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000304.sh &
export ID0000306=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000306
export ID0000306_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000306.sh &
export ID0000308=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000308
export ID0000308_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000308.sh &
export ID0000310=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000310
export ID0000310_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000310.sh &
export ID0000312=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000312
export ID0000312_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000312.sh &
export ID0000314=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000314
export ID0000314_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000314.sh &
export ID0000316=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000316
export ID0000316_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000316.sh &
export ID0000318=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000318
export ID0000318_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000318.sh &
export ID0000320=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000320
export ID0000320_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000320.sh &
export ID0000322=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000322
export ID0000322_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000322.sh &
export ID0000324=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000324
export ID0000324_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000324.sh &
export ID0000326=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000326
export ID0000326_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000326.sh &
export ID0000328=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000328
export ID0000328_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000328.sh &
export ID0000330=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000330
export ID0000330_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000330.sh &
export ID0000332=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000332
export ID0000332_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000332.sh &
export ID0000334=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000334
export ID0000334_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000334.sh &
export ID0000336=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000336
export ID0000336_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000336.sh &
export ID0000338=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000338
export ID0000338_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000338.sh &
export ID0000340=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000340
export ID0000340_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000340.sh &
export ID0000342=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000342
export ID0000342_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000342.sh &
export ID0000344=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000344
export ID0000344_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000344.sh &
export ID0000346=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000346
export ID0000346_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000346.sh &
export ID0000348=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000348
export ID0000348_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000348.sh &
export ID0000350=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000350
export ID0000350_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000350.sh &
export ID0000352=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000352
export ID0000352_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000352.sh &
export ID0000354=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000354
export ID0000354_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000354.sh &
export ID0000356=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000356
export ID0000356_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000356.sh &
export ID0000358=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000358
export ID0000358_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000358.sh &
export ID0000360=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000360
export ID0000360_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000360.sh &
export ID0000362=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000362
export ID0000362_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000362.sh &
export ID0000364=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000364
export ID0000364_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000364.sh &
export ID0000366=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000366
export ID0000366_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000366.sh &
export ID0000368=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000368
export ID0000368_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000368.sh &
export ID0000370=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000370
export ID0000370_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000370.sh &
export ID0000372=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000372
export ID0000372_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000372.sh &
export ID0000374=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000374
export ID0000374_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000374.sh &
export ID0000376=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000376
export ID0000376_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000376.sh &
export ID0000378=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000378
export ID0000378_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000378.sh &
export ID0000380=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000380
export ID0000380_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000380.sh &
export ID0000382=/$NODE_LOCAL/fasterq_dump_wrapper_ID0000382
export ID0000382_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000382.sh &
export ID0000384=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/fasterq_dump_wrapper_ID0000384
export ID0000384_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/fasterq_dump_wrapper_ID0000384.sh &
wait
duration=$(( $(date +%s%3N) - $stage_start ))
echo fasterq_dump_wrapper, $duration >> stage_stat.log
stage_start=$(date +%s%3N)
export ID0000003=/$NODE_LOCAL/bowtie2_wrapper_ID0000003
export ID0000003_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000003.sh &
export ID0000005=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000005
export ID0000005_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000005.sh &
export ID0000007=/$NODE_LOCAL/bowtie2_wrapper_ID0000007
export ID0000007_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000007.sh &
export ID0000009=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000009
export ID0000009_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000009.sh &
export ID0000011=/$NODE_LOCAL/bowtie2_wrapper_ID0000011
export ID0000011_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000011.sh &
export ID0000013=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000013
export ID0000013_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000013.sh &
export ID0000015=/$NODE_LOCAL/bowtie2_wrapper_ID0000015
export ID0000015_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000015.sh &
export ID0000017=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000017
export ID0000017_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000017.sh &
export ID0000019=/$NODE_LOCAL/bowtie2_wrapper_ID0000019
export ID0000019_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000019.sh &
export ID0000021=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000021
export ID0000021_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000021.sh &
export ID0000023=/$NODE_LOCAL/bowtie2_wrapper_ID0000023
export ID0000023_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000023.sh &
export ID0000025=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000025
export ID0000025_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000025.sh &
export ID0000027=/$NODE_LOCAL/bowtie2_wrapper_ID0000027
export ID0000027_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000027.sh &
export ID0000029=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000029
export ID0000029_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000029.sh &
export ID0000031=/$NODE_LOCAL/bowtie2_wrapper_ID0000031
export ID0000031_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000031.sh &
export ID0000033=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000033
export ID0000033_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000033.sh &
export ID0000035=/$NODE_LOCAL/bowtie2_wrapper_ID0000035
export ID0000035_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000035.sh &
export ID0000037=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000037
export ID0000037_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000037.sh &
export ID0000039=/$NODE_LOCAL/bowtie2_wrapper_ID0000039
export ID0000039_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000039.sh &
export ID0000041=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000041
export ID0000041_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000041.sh &
export ID0000043=/$NODE_LOCAL/bowtie2_wrapper_ID0000043
export ID0000043_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000043.sh &
export ID0000045=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000045
export ID0000045_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000045.sh &
export ID0000047=/$NODE_LOCAL/bowtie2_wrapper_ID0000047
export ID0000047_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000047.sh &
export ID0000049=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000049
export ID0000049_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000049.sh &
export ID0000051=/$NODE_LOCAL/bowtie2_wrapper_ID0000051
export ID0000051_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000051.sh &
export ID0000053=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000053
export ID0000053_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000053.sh &
export ID0000055=/$NODE_LOCAL/bowtie2_wrapper_ID0000055
export ID0000055_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000055.sh &
export ID0000057=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000057
export ID0000057_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000057.sh &
export ID0000059=/$NODE_LOCAL/bowtie2_wrapper_ID0000059
export ID0000059_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000059.sh &
export ID0000061=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000061
export ID0000061_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000061.sh &
export ID0000063=/$NODE_LOCAL/bowtie2_wrapper_ID0000063
export ID0000063_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000063.sh &
export ID0000065=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000065
export ID0000065_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000065.sh &
export ID0000067=/$NODE_LOCAL/bowtie2_wrapper_ID0000067
export ID0000067_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000067.sh &
export ID0000069=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000069
export ID0000069_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000069.sh &
export ID0000071=/$NODE_LOCAL/bowtie2_wrapper_ID0000071
export ID0000071_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000071.sh &
export ID0000073=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000073
export ID0000073_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000073.sh &
export ID0000075=/$NODE_LOCAL/bowtie2_wrapper_ID0000075
export ID0000075_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000075.sh &
export ID0000077=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000077
export ID0000077_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000077.sh &
export ID0000079=/$NODE_LOCAL/bowtie2_wrapper_ID0000079
export ID0000079_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000079.sh &
export ID0000081=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000081
export ID0000081_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000081.sh &
export ID0000083=/$NODE_LOCAL/bowtie2_wrapper_ID0000083
export ID0000083_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000083.sh &
export ID0000085=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000085
export ID0000085_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000085.sh &
export ID0000087=/$NODE_LOCAL/bowtie2_wrapper_ID0000087
export ID0000087_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000087.sh &
export ID0000089=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000089
export ID0000089_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000089.sh &
export ID0000091=/$NODE_LOCAL/bowtie2_wrapper_ID0000091
export ID0000091_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000091.sh &
export ID0000093=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000093
export ID0000093_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000093.sh &
export ID0000095=/$NODE_LOCAL/bowtie2_wrapper_ID0000095
export ID0000095_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000095.sh &
export ID0000097=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000097
export ID0000097_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000097.sh &
export ID0000099=/$NODE_LOCAL/bowtie2_wrapper_ID0000099
export ID0000099_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000099.sh &
export ID0000101=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000101
export ID0000101_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000101.sh &
export ID0000103=/$NODE_LOCAL/bowtie2_wrapper_ID0000103
export ID0000103_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000103.sh &
export ID0000105=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000105
export ID0000105_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000105.sh &
export ID0000107=/$NODE_LOCAL/bowtie2_wrapper_ID0000107
export ID0000107_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000107.sh &
export ID0000109=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000109
export ID0000109_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000109.sh &
export ID0000111=/$NODE_LOCAL/bowtie2_wrapper_ID0000111
export ID0000111_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000111.sh &
export ID0000113=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000113
export ID0000113_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000113.sh &
export ID0000115=/$NODE_LOCAL/bowtie2_wrapper_ID0000115
export ID0000115_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000115.sh &
export ID0000117=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000117
export ID0000117_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000117.sh &
export ID0000119=/$NODE_LOCAL/bowtie2_wrapper_ID0000119
export ID0000119_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000119.sh &
export ID0000121=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000121
export ID0000121_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000121.sh &
export ID0000123=/$NODE_LOCAL/bowtie2_wrapper_ID0000123
export ID0000123_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000123.sh &
export ID0000125=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000125
export ID0000125_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000125.sh &
export ID0000127=/$NODE_LOCAL/bowtie2_wrapper_ID0000127
export ID0000127_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000127.sh &
export ID0000129=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000129
export ID0000129_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000129.sh &
export ID0000131=/$NODE_LOCAL/bowtie2_wrapper_ID0000131
export ID0000131_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000131.sh &
export ID0000133=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000133
export ID0000133_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000133.sh &
export ID0000135=/$NODE_LOCAL/bowtie2_wrapper_ID0000135
export ID0000135_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000135.sh &
export ID0000137=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000137
export ID0000137_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000137.sh &
export ID0000139=/$NODE_LOCAL/bowtie2_wrapper_ID0000139
export ID0000139_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000139.sh &
export ID0000141=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000141
export ID0000141_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000141.sh &
export ID0000143=/$NODE_LOCAL/bowtie2_wrapper_ID0000143
export ID0000143_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000143.sh &
export ID0000145=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000145
export ID0000145_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000145.sh &
export ID0000147=/$NODE_LOCAL/bowtie2_wrapper_ID0000147
export ID0000147_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000147.sh &
export ID0000149=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000149
export ID0000149_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000149.sh &
export ID0000151=/$NODE_LOCAL/bowtie2_wrapper_ID0000151
export ID0000151_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000151.sh &
export ID0000153=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000153
export ID0000153_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000153.sh &
export ID0000155=/$NODE_LOCAL/bowtie2_wrapper_ID0000155
export ID0000155_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000155.sh &
export ID0000157=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000157
export ID0000157_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000157.sh &
export ID0000159=/$NODE_LOCAL/bowtie2_wrapper_ID0000159
export ID0000159_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000159.sh &
export ID0000161=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000161
export ID0000161_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000161.sh &
export ID0000163=/$NODE_LOCAL/bowtie2_wrapper_ID0000163
export ID0000163_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000163.sh &
export ID0000165=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000165
export ID0000165_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000165.sh &
export ID0000167=/$NODE_LOCAL/bowtie2_wrapper_ID0000167
export ID0000167_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000167.sh &
export ID0000169=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000169
export ID0000169_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000169.sh &
export ID0000171=/$NODE_LOCAL/bowtie2_wrapper_ID0000171
export ID0000171_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000171.sh &
export ID0000173=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000173
export ID0000173_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000173.sh &
export ID0000175=/$NODE_LOCAL/bowtie2_wrapper_ID0000175
export ID0000175_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000175.sh &
export ID0000177=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000177
export ID0000177_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000177.sh &
export ID0000179=/$NODE_LOCAL/bowtie2_wrapper_ID0000179
export ID0000179_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000179.sh &
export ID0000181=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000181
export ID0000181_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000181.sh &
export ID0000183=/$NODE_LOCAL/bowtie2_wrapper_ID0000183
export ID0000183_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000183.sh &
export ID0000185=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000185
export ID0000185_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000185.sh &
export ID0000187=/$NODE_LOCAL/bowtie2_wrapper_ID0000187
export ID0000187_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000187.sh &
export ID0000189=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000189
export ID0000189_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000189.sh &
export ID0000191=/$NODE_LOCAL/bowtie2_wrapper_ID0000191
export ID0000191_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000191.sh &
export ID0000193=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000193
export ID0000193_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000193.sh &
export ID0000195=/$NODE_LOCAL/bowtie2_wrapper_ID0000195
export ID0000195_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000195.sh &
export ID0000197=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000197
export ID0000197_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000197.sh &
export ID0000199=/$NODE_LOCAL/bowtie2_wrapper_ID0000199
export ID0000199_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000199.sh &
export ID0000201=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000201
export ID0000201_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000201.sh &
export ID0000203=/$NODE_LOCAL/bowtie2_wrapper_ID0000203
export ID0000203_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000203.sh &
export ID0000205=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000205
export ID0000205_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000205.sh &
export ID0000207=/$NODE_LOCAL/bowtie2_wrapper_ID0000207
export ID0000207_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000207.sh &
export ID0000209=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000209
export ID0000209_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000209.sh &
export ID0000211=/$NODE_LOCAL/bowtie2_wrapper_ID0000211
export ID0000211_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000211.sh &
export ID0000213=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000213
export ID0000213_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000213.sh &
export ID0000215=/$NODE_LOCAL/bowtie2_wrapper_ID0000215
export ID0000215_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000215.sh &
export ID0000217=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000217
export ID0000217_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000217.sh &
export ID0000219=/$NODE_LOCAL/bowtie2_wrapper_ID0000219
export ID0000219_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000219.sh &
export ID0000221=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000221
export ID0000221_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000221.sh &
export ID0000223=/$NODE_LOCAL/bowtie2_wrapper_ID0000223
export ID0000223_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000223.sh &
export ID0000225=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000225
export ID0000225_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000225.sh &
export ID0000227=/$NODE_LOCAL/bowtie2_wrapper_ID0000227
export ID0000227_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000227.sh &
export ID0000229=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000229
export ID0000229_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000229.sh &
export ID0000231=/$NODE_LOCAL/bowtie2_wrapper_ID0000231
export ID0000231_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000231.sh &
export ID0000233=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000233
export ID0000233_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000233.sh &
export ID0000235=/$NODE_LOCAL/bowtie2_wrapper_ID0000235
export ID0000235_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000235.sh &
export ID0000237=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000237
export ID0000237_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000237.sh &
export ID0000239=/$NODE_LOCAL/bowtie2_wrapper_ID0000239
export ID0000239_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000239.sh &
export ID0000241=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000241
export ID0000241_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000241.sh &
export ID0000243=/$NODE_LOCAL/bowtie2_wrapper_ID0000243
export ID0000243_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000243.sh &
export ID0000245=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000245
export ID0000245_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000245.sh &
export ID0000247=/$NODE_LOCAL/bowtie2_wrapper_ID0000247
export ID0000247_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000247.sh &
export ID0000249=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000249
export ID0000249_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000249.sh &
export ID0000251=/$NODE_LOCAL/bowtie2_wrapper_ID0000251
export ID0000251_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000251.sh &
export ID0000253=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000253
export ID0000253_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000253.sh &
export ID0000255=/$NODE_LOCAL/bowtie2_wrapper_ID0000255
export ID0000255_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000255.sh &
export ID0000257=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000257
export ID0000257_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000257.sh &
export ID0000259=/$NODE_LOCAL/bowtie2_wrapper_ID0000259
export ID0000259_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000259.sh &
export ID0000261=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000261
export ID0000261_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000261.sh &
export ID0000263=/$NODE_LOCAL/bowtie2_wrapper_ID0000263
export ID0000263_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000263.sh &
export ID0000265=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000265
export ID0000265_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000265.sh &
export ID0000267=/$NODE_LOCAL/bowtie2_wrapper_ID0000267
export ID0000267_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000267.sh &
export ID0000269=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000269
export ID0000269_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000269.sh &
export ID0000271=/$NODE_LOCAL/bowtie2_wrapper_ID0000271
export ID0000271_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000271.sh &
export ID0000273=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000273
export ID0000273_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000273.sh &
export ID0000275=/$NODE_LOCAL/bowtie2_wrapper_ID0000275
export ID0000275_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000275.sh &
export ID0000277=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000277
export ID0000277_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000277.sh &
export ID0000279=/$NODE_LOCAL/bowtie2_wrapper_ID0000279
export ID0000279_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000279.sh &
export ID0000281=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000281
export ID0000281_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000281.sh &
export ID0000283=/$NODE_LOCAL/bowtie2_wrapper_ID0000283
export ID0000283_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000283.sh &
export ID0000285=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000285
export ID0000285_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000285.sh &
export ID0000287=/$NODE_LOCAL/bowtie2_wrapper_ID0000287
export ID0000287_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000287.sh &
export ID0000289=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000289
export ID0000289_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000289.sh &
export ID0000291=/$NODE_LOCAL/bowtie2_wrapper_ID0000291
export ID0000291_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000291.sh &
export ID0000293=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000293
export ID0000293_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000293.sh &
export ID0000295=/$NODE_LOCAL/bowtie2_wrapper_ID0000295
export ID0000295_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000295.sh &
export ID0000297=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000297
export ID0000297_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000297.sh &
export ID0000299=/$NODE_LOCAL/bowtie2_wrapper_ID0000299
export ID0000299_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000299.sh &
export ID0000301=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000301
export ID0000301_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000301.sh &
export ID0000303=/$NODE_LOCAL/bowtie2_wrapper_ID0000303
export ID0000303_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000303.sh &
export ID0000305=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000305
export ID0000305_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000305.sh &
export ID0000307=/$NODE_LOCAL/bowtie2_wrapper_ID0000307
export ID0000307_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000307.sh &
export ID0000309=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000309
export ID0000309_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000309.sh &
export ID0000311=/$NODE_LOCAL/bowtie2_wrapper_ID0000311
export ID0000311_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000311.sh &
export ID0000313=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000313
export ID0000313_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000313.sh &
export ID0000315=/$NODE_LOCAL/bowtie2_wrapper_ID0000315
export ID0000315_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000315.sh &
export ID0000317=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000317
export ID0000317_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000317.sh &
export ID0000319=/$NODE_LOCAL/bowtie2_wrapper_ID0000319
export ID0000319_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000319.sh &
export ID0000321=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000321
export ID0000321_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000321.sh &
export ID0000323=/$NODE_LOCAL/bowtie2_wrapper_ID0000323
export ID0000323_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000323.sh &
export ID0000325=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000325
export ID0000325_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000325.sh &
export ID0000327=/$NODE_LOCAL/bowtie2_wrapper_ID0000327
export ID0000327_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000327.sh &
export ID0000329=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000329
export ID0000329_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000329.sh &
export ID0000331=/$NODE_LOCAL/bowtie2_wrapper_ID0000331
export ID0000331_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000331.sh &
export ID0000333=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000333
export ID0000333_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000333.sh &
export ID0000335=/$NODE_LOCAL/bowtie2_wrapper_ID0000335
export ID0000335_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000335.sh &
export ID0000337=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000337
export ID0000337_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000337.sh &
export ID0000339=/$NODE_LOCAL/bowtie2_wrapper_ID0000339
export ID0000339_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000339.sh &
export ID0000341=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000341
export ID0000341_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000341.sh &
export ID0000343=/$NODE_LOCAL/bowtie2_wrapper_ID0000343
export ID0000343_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000343.sh &
export ID0000345=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000345
export ID0000345_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000345.sh &
export ID0000347=/$NODE_LOCAL/bowtie2_wrapper_ID0000347
export ID0000347_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000347.sh &
export ID0000349=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000349
export ID0000349_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000349.sh &
export ID0000351=/$NODE_LOCAL/bowtie2_wrapper_ID0000351
export ID0000351_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000351.sh &
export ID0000353=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000353
export ID0000353_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000353.sh &
export ID0000355=/$NODE_LOCAL/bowtie2_wrapper_ID0000355
export ID0000355_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000355.sh &
export ID0000357=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000357
export ID0000357_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000357.sh &
export ID0000359=/$NODE_LOCAL/bowtie2_wrapper_ID0000359
export ID0000359_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000359.sh &
export ID0000361=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000361
export ID0000361_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000361.sh &
export ID0000363=/$NODE_LOCAL/bowtie2_wrapper_ID0000363
export ID0000363_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000363.sh &
export ID0000365=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000365
export ID0000365_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000365.sh &
export ID0000367=/$NODE_LOCAL/bowtie2_wrapper_ID0000367
export ID0000367_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000367.sh &
export ID0000369=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000369
export ID0000369_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000369.sh &
export ID0000371=/$NODE_LOCAL/bowtie2_wrapper_ID0000371
export ID0000371_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000371.sh &
export ID0000373=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000373
export ID0000373_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000373.sh &
export ID0000375=/$NODE_LOCAL/bowtie2_wrapper_ID0000375
export ID0000375_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000375.sh &
export ID0000377=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000377
export ID0000377_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000377.sh &
export ID0000379=/$NODE_LOCAL/bowtie2_wrapper_ID0000379
export ID0000379_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000379.sh &
export ID0000381=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000381
export ID0000381_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000381.sh &
export ID0000383=/$NODE_LOCAL/bowtie2_wrapper_ID0000383
export ID0000383_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000383.sh &
export ID0000385=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/bowtie2_wrapper_ID0000385
export ID0000385_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/bowtie2_wrapper_ID0000385.sh &
wait
duration=$(( $(date +%s%3N) - $stage_start ))
echo bowtie2_wrapper, $duration >> stage_stat.log
stage_start=$(date +%s%3N)
export ID0000386=/$NODE_LOCAL/merge_ID0000386
export ID0000386_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000386.sh &
export ID0000387=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/merge_ID0000387
export ID0000387_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000387.sh &
export ID0000388=/$NODE_LOCAL/merge_ID0000388
export ID0000388_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000388.sh &
export ID0000389=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/merge_ID0000389
export ID0000389_node=$node3
srun -w $node3 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000389.sh &
export ID0000390=/$NODE_LOCAL/merge_ID0000390
export ID0000390_node=$node4
srun -w $node4 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000390.sh &
export ID0000391=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/merge_ID0000391
export ID0000391_node=$node5
srun -w $node5 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000391.sh &
export ID0000392=/$NODE_LOCAL/merge_ID0000392
export ID0000392_node=$node6
srun -w $node6 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000392.sh &
export ID0000393=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/merge_ID0000393
export ID0000393_node=$node7
srun -w $node7 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000393.sh &
export ID0000394=/$NODE_LOCAL/merge_ID0000394
export ID0000394_node=$node8
srun -w $node8 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000394.sh &
export ID0000395=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/merge_ID0000395
export ID0000395_node=$node9
srun -w $node9 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000395.sh &
export ID0000396=/$NODE_LOCAL/merge_ID0000396
export ID0000396_node=$node10
srun -w $node10 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000396.sh &
export ID0000397=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/merge_ID0000397
export ID0000397_node=$node11
srun -w $node11 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000397.sh &
export ID0000398=/$NODE_LOCAL/merge_ID0000398
export ID0000398_node=$node12
srun -w $node12 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000398.sh &
export ID0000399=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/merge_ID0000399
export ID0000399_node=$node13
srun -w $node13 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000399.sh &
export ID0000400=/$NODE_LOCAL/merge_ID0000400
export ID0000400_node=$node14
srun -w $node14 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000400.sh &
export ID0000401=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/merge_ID0000401
export ID0000401_node=$node15
srun -w $node15 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000401.sh &
export ID0000402=/$NODE_LOCAL/merge_ID0000402
export ID0000402_node=$node0
srun -w $node0 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000402.sh &
export ID0000403=/$PFS_PATH/sra-search-workflow/192sras_large/16nodes/cpop_50/merge_ID0000403
export ID0000403_node=$node1
srun -w $node1 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000403.sh &
export ID0000404=/$NODE_LOCAL/merge_ID0000404
export ID0000404_node=$node2
srun -w $node2 -n1 -N1 --exclusive bash flow_sequential/merge_ID0000404.sh &
wait
duration=$(( $(date +%s%3N) - $stage_start ))
echo merge, $duration >> stage_stat.log
