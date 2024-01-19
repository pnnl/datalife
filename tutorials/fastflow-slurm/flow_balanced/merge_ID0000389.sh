mkdir -p $NODE_LOCAL/merge_ID0000389
cd $NODE_LOCAL/merge_ID0000389
if [ -e $ID0000085/SRR1518569.bam.bai ]; then  /bin/ln -s $ID0000085/SRR1518569.bam.bai ./  ; else scp $ID0000085_node:$ID0000085/SRR1518569.bam.bai ./ ; fi
if [ -e $ID0000097/SRR1518575.bam ]; then  /bin/ln -s $ID0000097/SRR1518575.bam ./  ; else scp $ID0000097_node:$ID0000097/SRR1518575.bam ./ ; fi
if [ -e $ID0000079/SRR1518537.bam.bai ]; then  /bin/ln -s $ID0000079/SRR1518537.bam.bai ./  ; else scp $ID0000079_node:$ID0000079/SRR1518537.bam.bai ./ ; fi
if [ -e $ID0000093/SRR1518570.bam.bai ]; then  /bin/ln -s $ID0000093/SRR1518570.bam.bai ./  ; else scp $ID0000093_node:$ID0000093/SRR1518570.bam.bai ./ ; fi
if [ -e $ID0000075/SRR1518952.bam.bai ]; then  /bin/ln -s $ID0000075/SRR1518952.bam.bai ./  ; else scp $ID0000075_node:$ID0000075/SRR1518952.bam.bai ./ ; fi
if [ -e $ID0000081/SRR1763637.bam ]; then  /bin/ln -s $ID0000081/SRR1763637.bam ./  ; else scp $ID0000081_node:$ID0000081/SRR1763637.bam ./ ; fi
if [ -e $ID0000077/SRR1518579.bam.bai ]; then  /bin/ln -s $ID0000077/SRR1518579.bam.bai ./  ; else scp $ID0000077_node:$ID0000077/SRR1518579.bam.bai ./ ; fi
if [ -e $ID0000087/SRR1518577.bam.bai ]; then  /bin/ln -s $ID0000087/SRR1518577.bam.bai ./  ; else scp $ID0000087_node:$ID0000087/SRR1518577.bam.bai ./ ; fi
if [ -e $ID0000095/SRR1518965.bam ]; then  /bin/ln -s $ID0000095/SRR1518965.bam ./  ; else scp $ID0000095_node:$ID0000095/SRR1518965.bam ./ ; fi
if [ -e $ID0000093/SRR1518570.bam ]; then  /bin/ln -s $ID0000093/SRR1518570.bam ./  ; else scp $ID0000093_node:$ID0000093/SRR1518570.bam ./ ; fi
if [ -e $ID0000085/SRR1518569.bam ]; then  /bin/ln -s $ID0000085/SRR1518569.bam ./  ; else scp $ID0000085_node:$ID0000085/SRR1518569.bam ./ ; fi
if [ -e $ID0000089/SRR1518546.bam ]; then  /bin/ln -s $ID0000089/SRR1518546.bam ./  ; else scp $ID0000089_node:$ID0000089/SRR1518546.bam ./ ; fi
if [ -e $ID0000083/SRR1518924.bam ]; then  /bin/ln -s $ID0000083/SRR1518924.bam ./  ; else scp $ID0000083_node:$ID0000083/SRR1518924.bam ./ ; fi
if [ -e $ID0000083/SRR1518924.bam.bai ]; then  /bin/ln -s $ID0000083/SRR1518924.bam.bai ./  ; else scp $ID0000083_node:$ID0000083/SRR1518924.bam.bai ./ ; fi
if [ -e $ID0000081/SRR1763637.bam.bai ]; then  /bin/ln -s $ID0000081/SRR1763637.bam.bai ./  ; else scp $ID0000081_node:$ID0000081/SRR1763637.bam.bai ./ ; fi
if [ -e $ID0000095/SRR1518965.bam.bai ]; then  /bin/ln -s $ID0000095/SRR1518965.bam.bai ./  ; else scp $ID0000095_node:$ID0000095/SRR1518965.bam.bai ./ ; fi
if [ -e $ID0000079/SRR1518537.bam ]; then  /bin/ln -s $ID0000079/SRR1518537.bam ./  ; else scp $ID0000079_node:$ID0000079/SRR1518537.bam ./ ; fi
if [ -e $ID0000075/SRR1518952.bam ]; then  /bin/ln -s $ID0000075/SRR1518952.bam ./  ; else scp $ID0000075_node:$ID0000075/SRR1518952.bam ./ ; fi
if [ -e $ID0000091/SRR3150340.bam.bai ]; then  /bin/ln -s $ID0000091/SRR3150340.bam.bai ./  ; else scp $ID0000091_node:$ID0000091/SRR3150340.bam.bai ./ ; fi
if [ -e $ID0000091/SRR3150340.bam ]; then  /bin/ln -s $ID0000091/SRR3150340.bam ./  ; else scp $ID0000091_node:$ID0000091/SRR3150340.bam ./ ; fi
if [ -e $ID0000077/SRR1518579.bam ]; then  /bin/ln -s $ID0000077/SRR1518579.bam ./  ; else scp $ID0000077_node:$ID0000077/SRR1518579.bam ./ ; fi
if [ -e $ID0000097/SRR1518575.bam.bai ]; then  /bin/ln -s $ID0000097/SRR1518575.bam.bai ./  ; else scp $ID0000097_node:$ID0000097/SRR1518575.bam.bai ./ ; fi
if [ -e $ID0000087/SRR1518577.bam ]; then  /bin/ln -s $ID0000087/SRR1518577.bam ./  ; else scp $ID0000087_node:$ID0000087/SRR1518577.bam ./ ; fi
if [ -e $ID0000089/SRR1518546.bam.bai ]; then  /bin/ln -s $ID0000089/SRR1518546.bam.bai ./  ; else scp $ID0000089_node:$ID0000089/SRR1518546.bam.bai ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results-l1-j4.tar.gz SRR1518952.bam.bai SRR1518952.bam SRR1518579.bam SRR1518579.bam.bai SRR1518537.bam SRR1518537.bam.bai SRR1763637.bam.bai SRR1763637.bam SRR1518924.bam SRR1518924.bam.bai SRR1518569.bam SRR1518569.bam.bai SRR1518577.bam SRR1518577.bam.bai SRR1518546.bam.bai SRR1518546.bam SRR3150340.bam.bai SRR3150340.bam SRR1518570.bam.bai SRR1518570.bam SRR1518965.bam.bai SRR1518965.bam SRR1518575.bam SRR1518575.bam.bai 
duration=$(( $(date +%s%3N) - start ))
echo ID0000389,merge,$duration,$start,$node3 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
