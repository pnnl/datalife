mkdir -p $NODE_LOCAL/merge_ID0000394
cd $NODE_LOCAL/merge_ID0000394
if [ -e $ID0000215/SRR1518647.bam.bai ]; then  /bin/ln -s $ID0000215/SRR1518647.bam.bai ./  ; else scp $ID0000215_node:$ID0000215/SRR1518647.bam.bai ./ ; fi
if [ -e $ID0000217/SRR1518564.bam ]; then  /bin/ln -s $ID0000217/SRR1518564.bam ./  ; else scp $ID0000217_node:$ID0000217/SRR1518564.bam ./ ; fi
if [ -e $ID0000203/SRR3177845.bam ]; then  /bin/ln -s $ID0000203/SRR3177845.bam ./  ; else scp $ID0000203_node:$ID0000203/SRR3177845.bam ./ ; fi
if [ -e $ID0000209/SRR1518469.bam ]; then  /bin/ln -s $ID0000209/SRR1518469.bam ./  ; else scp $ID0000209_node:$ID0000209/SRR1518469.bam ./ ; fi
if [ -e $ID0000207/SRR1518922.bam ]; then  /bin/ln -s $ID0000207/SRR1518922.bam ./  ; else scp $ID0000207_node:$ID0000207/SRR1518922.bam ./ ; fi
if [ -e $ID0000215/SRR1518647.bam ]; then  /bin/ln -s $ID0000215/SRR1518647.bam ./  ; else scp $ID0000215_node:$ID0000215/SRR1518647.bam ./ ; fi
if [ -e $ID0000211/SRR1518450.bam.bai ]; then  /bin/ln -s $ID0000211/SRR1518450.bam.bai ./  ; else scp $ID0000211_node:$ID0000211/SRR1518450.bam.bai ./ ; fi
if [ -e $ID0000201/SRR1518451.bam ]; then  /bin/ln -s $ID0000201/SRR1518451.bam ./  ; else scp $ID0000201_node:$ID0000201/SRR1518451.bam ./ ; fi
if [ -e $ID0000199/SRR1518948.bam ]; then  /bin/ln -s $ID0000199/SRR1518948.bam ./  ; else scp $ID0000199_node:$ID0000199/SRR1518948.bam ./ ; fi
if [ -e $ID0000217/SRR1518564.bam.bai ]; then  /bin/ln -s $ID0000217/SRR1518564.bam.bai ./  ; else scp $ID0000217_node:$ID0000217/SRR1518564.bam.bai ./ ; fi
if [ -e $ID0000205/SRR3150349.bam.bai ]; then  /bin/ln -s $ID0000205/SRR3150349.bam.bai ./  ; else scp $ID0000205_node:$ID0000205/SRR3150349.bam.bai ./ ; fi
if [ -e $ID0000201/SRR1518451.bam.bai ]; then  /bin/ln -s $ID0000201/SRR1518451.bam.bai ./  ; else scp $ID0000201_node:$ID0000201/SRR1518451.bam.bai ./ ; fi
if [ -e $ID0000197/SRR1518403.bam ]; then  /bin/ln -s $ID0000197/SRR1518403.bam ./  ; else scp $ID0000197_node:$ID0000197/SRR1518403.bam ./ ; fi
if [ -e $ID0000205/SRR3150349.bam ]; then  /bin/ln -s $ID0000205/SRR3150349.bam ./  ; else scp $ID0000205_node:$ID0000205/SRR3150349.bam ./ ; fi
if [ -e $ID0000195/SRR1918783.bam.bai ]; then  /bin/ln -s $ID0000195/SRR1918783.bam.bai ./  ; else scp $ID0000195_node:$ID0000195/SRR1918783.bam.bai ./ ; fi
if [ -e $ID0000195/SRR1918783.bam ]; then  /bin/ln -s $ID0000195/SRR1918783.bam ./  ; else scp $ID0000195_node:$ID0000195/SRR1918783.bam ./ ; fi
if [ -e $ID0000199/SRR1518948.bam.bai ]; then  /bin/ln -s $ID0000199/SRR1518948.bam.bai ./  ; else scp $ID0000199_node:$ID0000199/SRR1518948.bam.bai ./ ; fi
if [ -e $ID0000211/SRR1518450.bam ]; then  /bin/ln -s $ID0000211/SRR1518450.bam ./  ; else scp $ID0000211_node:$ID0000211/SRR1518450.bam ./ ; fi
if [ -e $ID0000213/SRR1518921.bam.bai ]; then  /bin/ln -s $ID0000213/SRR1518921.bam.bai ./  ; else scp $ID0000213_node:$ID0000213/SRR1518921.bam.bai ./ ; fi
if [ -e $ID0000213/SRR1518921.bam ]; then  /bin/ln -s $ID0000213/SRR1518921.bam ./  ; else scp $ID0000213_node:$ID0000213/SRR1518921.bam ./ ; fi
if [ -e $ID0000209/SRR1518469.bam.bai ]; then  /bin/ln -s $ID0000209/SRR1518469.bam.bai ./  ; else scp $ID0000209_node:$ID0000209/SRR1518469.bam.bai ./ ; fi
if [ -e $ID0000203/SRR3177845.bam.bai ]; then  /bin/ln -s $ID0000203/SRR3177845.bam.bai ./  ; else scp $ID0000203_node:$ID0000203/SRR3177845.bam.bai ./ ; fi
if [ -e $ID0000207/SRR1518922.bam.bai ]; then  /bin/ln -s $ID0000207/SRR1518922.bam.bai ./  ; else scp $ID0000207_node:$ID0000207/SRR1518922.bam.bai ./ ; fi
if [ -e $ID0000197/SRR1518403.bam.bai ]; then  /bin/ln -s $ID0000197/SRR1518403.bam.bai ./  ; else scp $ID0000197_node:$ID0000197/SRR1518403.bam.bai ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results-l1-j9.tar.gz SRR1918783.bam.bai SRR1918783.bam SRR1518403.bam SRR1518403.bam.bai SRR1518948.bam SRR1518948.bam.bai SRR1518451.bam SRR1518451.bam.bai SRR3177845.bam.bai SRR3177845.bam SRR3150349.bam SRR3150349.bam.bai SRR1518922.bam SRR1518922.bam.bai SRR1518469.bam.bai SRR1518469.bam SRR1518450.bam SRR1518450.bam.bai SRR1518921.bam SRR1518921.bam.bai SRR1518647.bam.bai SRR1518647.bam SRR1518564.bam SRR1518564.bam.bai 
duration=$(( $(date +%s%3N) - start ))
echo ID0000394,merge,$duration,$start,$node8 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
