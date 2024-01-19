mkdir -p $NODE_LOCAL/merge_ID0000390
cd $NODE_LOCAL/merge_ID0000390
if [ -e $ID0000117/SRR1518651.bam.bai ]; then  /bin/ln -s $ID0000117/SRR1518651.bam.bai ./  ; else scp $ID0000117_node:$ID0000117/SRR1518651.bam.bai ./ ; fi
if [ -e $ID0000105/SRR1518644.bam ]; then  /bin/ln -s $ID0000105/SRR1518644.bam ./  ; else scp $ID0000105_node:$ID0000105/SRR1518644.bam ./ ; fi
if [ -e $ID0000109/SRR3150392.bam.bai ]; then  /bin/ln -s $ID0000109/SRR3150392.bam.bai ./  ; else scp $ID0000109_node:$ID0000109/SRR3150392.bam.bai ./ ; fi
if [ -e $ID0000113/SRR1518634.bam ]; then  /bin/ln -s $ID0000113/SRR1518634.bam ./  ; else scp $ID0000113_node:$ID0000113/SRR1518634.bam ./ ; fi
if [ -e $ID0000107/SRR1518625.bam.bai ]; then  /bin/ln -s $ID0000107/SRR1518625.bam.bai ./  ; else scp $ID0000107_node:$ID0000107/SRR1518625.bam.bai ./ ; fi
if [ -e $ID0000107/SRR1518625.bam ]; then  /bin/ln -s $ID0000107/SRR1518625.bam ./  ; else scp $ID0000107_node:$ID0000107/SRR1518625.bam ./ ; fi
if [ -e $ID0000115/SRR1518467.bam.bai ]; then  /bin/ln -s $ID0000115/SRR1518467.bam.bai ./  ; else scp $ID0000115_node:$ID0000115/SRR1518467.bam.bai ./ ; fi
if [ -e $ID0000121/SRR3177800.bam.bai ]; then  /bin/ln -s $ID0000121/SRR3177800.bam.bai ./  ; else scp $ID0000121_node:$ID0000121/SRR3177800.bam.bai ./ ; fi
if [ -e $ID0000101/SRR1518641.bam ]; then  /bin/ln -s $ID0000101/SRR1518641.bam ./  ; else scp $ID0000101_node:$ID0000101/SRR1518641.bam ./ ; fi
if [ -e $ID0000115/SRR1518467.bam ]; then  /bin/ln -s $ID0000115/SRR1518467.bam ./  ; else scp $ID0000115_node:$ID0000115/SRR1518467.bam ./ ; fi
if [ -e $ID0000117/SRR1518651.bam ]; then  /bin/ln -s $ID0000117/SRR1518651.bam ./  ; else scp $ID0000117_node:$ID0000117/SRR1518651.bam ./ ; fi
if [ -e $ID0000111/SRR3177954.bam.bai ]; then  /bin/ln -s $ID0000111/SRR3177954.bam.bai ./  ; else scp $ID0000111_node:$ID0000111/SRR3177954.bam.bai ./ ; fi
if [ -e $ID0000103/SRR1518660.bam.bai ]; then  /bin/ln -s $ID0000103/SRR1518660.bam.bai ./  ; else scp $ID0000103_node:$ID0000103/SRR1518660.bam.bai ./ ; fi
if [ -e $ID0000099/SRR1763626.bam.bai ]; then  /bin/ln -s $ID0000099/SRR1763626.bam.bai ./  ; else scp $ID0000099_node:$ID0000099/SRR1763626.bam.bai ./ ; fi
if [ -e $ID0000113/SRR1518634.bam.bai ]; then  /bin/ln -s $ID0000113/SRR1518634.bam.bai ./  ; else scp $ID0000113_node:$ID0000113/SRR1518634.bam.bai ./ ; fi
if [ -e $ID0000119/SRR3177821.bam.bai ]; then  /bin/ln -s $ID0000119/SRR3177821.bam.bai ./  ; else scp $ID0000119_node:$ID0000119/SRR3177821.bam.bai ./ ; fi
if [ -e $ID0000119/SRR3177821.bam ]; then  /bin/ln -s $ID0000119/SRR3177821.bam ./  ; else scp $ID0000119_node:$ID0000119/SRR3177821.bam ./ ; fi
if [ -e $ID0000099/SRR1763626.bam ]; then  /bin/ln -s $ID0000099/SRR1763626.bam ./  ; else scp $ID0000099_node:$ID0000099/SRR1763626.bam ./ ; fi
if [ -e $ID0000105/SRR1518644.bam.bai ]; then  /bin/ln -s $ID0000105/SRR1518644.bam.bai ./  ; else scp $ID0000105_node:$ID0000105/SRR1518644.bam.bai ./ ; fi
if [ -e $ID0000101/SRR1518641.bam.bai ]; then  /bin/ln -s $ID0000101/SRR1518641.bam.bai ./  ; else scp $ID0000101_node:$ID0000101/SRR1518641.bam.bai ./ ; fi
if [ -e $ID0000109/SRR3150392.bam ]; then  /bin/ln -s $ID0000109/SRR3150392.bam ./  ; else scp $ID0000109_node:$ID0000109/SRR3150392.bam ./ ; fi
if [ -e $ID0000111/SRR3177954.bam ]; then  /bin/ln -s $ID0000111/SRR3177954.bam ./  ; else scp $ID0000111_node:$ID0000111/SRR3177954.bam ./ ; fi
if [ -e $ID0000121/SRR3177800.bam ]; then  /bin/ln -s $ID0000121/SRR3177800.bam ./  ; else scp $ID0000121_node:$ID0000121/SRR3177800.bam ./ ; fi
if [ -e $ID0000103/SRR1518660.bam ]; then  /bin/ln -s $ID0000103/SRR1518660.bam ./  ; else scp $ID0000103_node:$ID0000103/SRR1518660.bam ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results-l1-j5.tar.gz SRR1763626.bam SRR1763626.bam.bai SRR1518641.bam SRR1518641.bam.bai SRR1518660.bam SRR1518660.bam.bai SRR1518644.bam.bai SRR1518644.bam SRR1518625.bam.bai SRR1518625.bam SRR3150392.bam SRR3150392.bam.bai SRR3177954.bam SRR3177954.bam.bai SRR1518634.bam.bai SRR1518634.bam SRR1518467.bam SRR1518467.bam.bai SRR1518651.bam SRR1518651.bam.bai SRR3177821.bam.bai SRR3177821.bam SRR3177800.bam SRR3177800.bam.bai 
duration=$(( $(date +%s%3N) - start ))
echo ID0000390,merge,$duration,$start,$node4 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
