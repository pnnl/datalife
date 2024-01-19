mkdir -p $NODE_LOCAL/merge_ID0000386
cd $NODE_LOCAL/merge_ID0000386
if [ -e $ID0000021/SRR3150816.bam ]; then  /bin/ln -s $ID0000021/SRR3150816.bam ./  ; else scp $ID0000021_node:$ID0000021/SRR3150816.bam ./ ; fi
if [ -e $ID0000015/SRR1763631.bam.bai ]; then  /bin/ln -s $ID0000015/SRR1763631.bam.bai ./  ; else scp $ID0000015_node:$ID0000015/SRR1763631.bam.bai ./ ; fi
if [ -e $ID0000005/SRR1918770.bam.bai ]; then  /bin/ln -s $ID0000005/SRR1918770.bam.bai ./  ; else scp $ID0000005_node:$ID0000005/SRR1918770.bam.bai ./ ; fi
if [ -e $ID0000025/SRR2010213.bam.bai ]; then  /bin/ln -s $ID0000025/SRR2010213.bam.bai ./  ; else scp $ID0000025_node:$ID0000025/SRR2010213.bam.bai ./ ; fi
if [ -e $ID0000023/SRR3150508.bam.bai ]; then  /bin/ln -s $ID0000023/SRR3150508.bam.bai ./  ; else scp $ID0000023_node:$ID0000023/SRR3150508.bam.bai ./ ; fi
if [ -e $ID0000003/SRR1518468.bam.bai ]; then  /bin/ln -s $ID0000003/SRR1518468.bam.bai ./  ; else scp $ID0000003_node:$ID0000003/SRR1518468.bam.bai ./ ; fi
if [ -e $ID0000007/SRR3177793.bam ]; then  /bin/ln -s $ID0000007/SRR3177793.bam ./  ; else scp $ID0000007_node:$ID0000007/SRR3177793.bam ./ ; fi
if [ -e $ID0000023/SRR3150508.bam ]; then  /bin/ln -s $ID0000023/SRR3150508.bam ./  ; else scp $ID0000023_node:$ID0000023/SRR3150508.bam ./ ; fi
if [ -e $ID0000013/SRR1922803.bam.bai ]; then  /bin/ln -s $ID0000013/SRR1922803.bam.bai ./  ; else scp $ID0000013_node:$ID0000013/SRR1922803.bam.bai ./ ; fi
if [ -e $ID0000019/SRR3177798.bam ]; then  /bin/ln -s $ID0000019/SRR3177798.bam ./  ; else scp $ID0000019_node:$ID0000019/SRR3177798.bam ./ ; fi
if [ -e $ID0000025/SRR2010213.bam ]; then  /bin/ln -s $ID0000025/SRR2010213.bam ./  ; else scp $ID0000025_node:$ID0000025/SRR2010213.bam ./ ; fi
if [ -e $ID0000005/SRR1918770.bam ]; then  /bin/ln -s $ID0000005/SRR1918770.bam ./  ; else scp $ID0000005_node:$ID0000005/SRR1918770.bam ./ ; fi
if [ -e $ID0000013/SRR1922803.bam ]; then  /bin/ln -s $ID0000013/SRR1922803.bam ./  ; else scp $ID0000013_node:$ID0000013/SRR1922803.bam ./ ; fi
if [ -e $ID0000003/SRR1518468.bam ]; then  /bin/ln -s $ID0000003/SRR1518468.bam ./  ; else scp $ID0000003_node:$ID0000003/SRR1518468.bam ./ ; fi
if [ -e $ID0000017/SRR1922713.bam ]; then  /bin/ln -s $ID0000017/SRR1922713.bam ./  ; else scp $ID0000017_node:$ID0000017/SRR1922713.bam ./ ; fi
if [ -e $ID0000019/SRR3177798.bam.bai ]; then  /bin/ln -s $ID0000019/SRR3177798.bam.bai ./  ; else scp $ID0000019_node:$ID0000019/SRR3177798.bam.bai ./ ; fi
if [ -e $ID0000021/SRR3150816.bam.bai ]; then  /bin/ln -s $ID0000021/SRR3150816.bam.bai ./  ; else scp $ID0000021_node:$ID0000021/SRR3150816.bam.bai ./ ; fi
if [ -e $ID0000015/SRR1763631.bam ]; then  /bin/ln -s $ID0000015/SRR1763631.bam ./  ; else scp $ID0000015_node:$ID0000015/SRR1763631.bam ./ ; fi
if [ -e $ID0000009/SRR1919983.bam.bai ]; then  /bin/ln -s $ID0000009/SRR1919983.bam.bai ./  ; else scp $ID0000009_node:$ID0000009/SRR1919983.bam.bai ./ ; fi
if [ -e $ID0000009/SRR1919983.bam ]; then  /bin/ln -s $ID0000009/SRR1919983.bam ./  ; else scp $ID0000009_node:$ID0000009/SRR1919983.bam ./ ; fi
if [ -e $ID0000017/SRR1922713.bam.bai ]; then  /bin/ln -s $ID0000017/SRR1922713.bam.bai ./  ; else scp $ID0000017_node:$ID0000017/SRR1922713.bam.bai ./ ; fi
if [ -e $ID0000011/SRR1918775.bam ]; then  /bin/ln -s $ID0000011/SRR1918775.bam ./  ; else scp $ID0000011_node:$ID0000011/SRR1918775.bam ./ ; fi
if [ -e $ID0000011/SRR1918775.bam.bai ]; then  /bin/ln -s $ID0000011/SRR1918775.bam.bai ./  ; else scp $ID0000011_node:$ID0000011/SRR1918775.bam.bai ./ ; fi
if [ -e $ID0000007/SRR3177793.bam.bai ]; then  /bin/ln -s $ID0000007/SRR3177793.bam.bai ./  ; else scp $ID0000007_node:$ID0000007/SRR3177793.bam.bai ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results-l1-j1.tar.gz SRR1518468.bam SRR1518468.bam.bai SRR1918770.bam SRR1918770.bam.bai SRR3177793.bam SRR3177793.bam.bai SRR1919983.bam.bai SRR1919983.bam SRR1918775.bam SRR1918775.bam.bai SRR1922803.bam.bai SRR1922803.bam SRR1763631.bam.bai SRR1763631.bam SRR1922713.bam SRR1922713.bam.bai SRR3177798.bam SRR3177798.bam.bai SRR3150816.bam SRR3150816.bam.bai SRR3150508.bam.bai SRR3150508.bam SRR2010213.bam.bai SRR2010213.bam 
duration=$(( $(date +%s%3N) - start ))
echo ID0000386,merge,$duration,$start,$node0 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
