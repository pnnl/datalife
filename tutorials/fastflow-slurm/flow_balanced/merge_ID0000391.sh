mkdir -p $NODE_LOCAL/merge_ID0000391
cd $NODE_LOCAL/merge_ID0000391
if [ -e $ID0000131/SRR3177888.bam.bai ]; then  /bin/ln -s $ID0000131/SRR3177888.bam.bai ./  ; else scp $ID0000131_node:$ID0000131/SRR3177888.bam.bai ./ ; fi
if [ -e $ID0000127/SRR3170548.bam ]; then  /bin/ln -s $ID0000127/SRR3170548.bam ./  ; else scp $ID0000127_node:$ID0000127/SRR3170548.bam ./ ; fi
if [ -e $ID0000123/SRR3150506.bam.bai ]; then  /bin/ln -s $ID0000123/SRR3150506.bam.bai ./  ; else scp $ID0000123_node:$ID0000123/SRR3150506.bam.bai ./ ; fi
if [ -e $ID0000145/SRR3150357.bam.bai ]; then  /bin/ln -s $ID0000145/SRR3150357.bam.bai ./  ; else scp $ID0000145_node:$ID0000145/SRR3150357.bam.bai ./ ; fi
if [ -e $ID0000131/SRR3177888.bam ]; then  /bin/ln -s $ID0000131/SRR3177888.bam ./  ; else scp $ID0000131_node:$ID0000131/SRR3177888.bam ./ ; fi
if [ -e $ID0000133/SRR2009780.bam.bai ]; then  /bin/ln -s $ID0000133/SRR2009780.bam.bai ./  ; else scp $ID0000133_node:$ID0000133/SRR2009780.bam.bai ./ ; fi
if [ -e $ID0000141/SRR1518481.bam ]; then  /bin/ln -s $ID0000141/SRR1518481.bam ./  ; else scp $ID0000141_node:$ID0000141/SRR1518481.bam ./ ; fi
if [ -e $ID0000123/SRR3150506.bam ]; then  /bin/ln -s $ID0000123/SRR3150506.bam ./  ; else scp $ID0000123_node:$ID0000123/SRR3150506.bam ./ ; fi
if [ -e $ID0000137/SRR1518933.bam ]; then  /bin/ln -s $ID0000137/SRR1518933.bam ./  ; else scp $ID0000137_node:$ID0000137/SRR1518933.bam ./ ; fi
if [ -e $ID0000133/SRR2009780.bam ]; then  /bin/ln -s $ID0000133/SRR2009780.bam ./  ; else scp $ID0000133_node:$ID0000133/SRR2009780.bam ./ ; fi
if [ -e $ID0000139/SRR1518447.bam ]; then  /bin/ln -s $ID0000139/SRR1518447.bam ./  ; else scp $ID0000139_node:$ID0000139/SRR1518447.bam ./ ; fi
if [ -e $ID0000129/SRR1518969.bam ]; then  /bin/ln -s $ID0000129/SRR1518969.bam ./  ; else scp $ID0000129_node:$ID0000129/SRR1518969.bam ./ ; fi
if [ -e $ID0000139/SRR1518447.bam.bai ]; then  /bin/ln -s $ID0000139/SRR1518447.bam.bai ./  ; else scp $ID0000139_node:$ID0000139/SRR1518447.bam.bai ./ ; fi
if [ -e $ID0000141/SRR1518481.bam.bai ]; then  /bin/ln -s $ID0000141/SRR1518481.bam.bai ./  ; else scp $ID0000141_node:$ID0000141/SRR1518481.bam.bai ./ ; fi
if [ -e $ID0000137/SRR1518933.bam.bai ]; then  /bin/ln -s $ID0000137/SRR1518933.bam.bai ./  ; else scp $ID0000137_node:$ID0000137/SRR1518933.bam.bai ./ ; fi
if [ -e $ID0000145/SRR3150357.bam ]; then  /bin/ln -s $ID0000145/SRR3150357.bam ./  ; else scp $ID0000145_node:$ID0000145/SRR3150357.bam ./ ; fi
if [ -e $ID0000129/SRR1518969.bam.bai ]; then  /bin/ln -s $ID0000129/SRR1518969.bam.bai ./  ; else scp $ID0000129_node:$ID0000129/SRR1518969.bam.bai ./ ; fi
if [ -e $ID0000143/SRR1518944.bam.bai ]; then  /bin/ln -s $ID0000143/SRR1518944.bam.bai ./  ; else scp $ID0000143_node:$ID0000143/SRR1518944.bam.bai ./ ; fi
if [ -e $ID0000125/SRR1518482.bam ]; then  /bin/ln -s $ID0000125/SRR1518482.bam ./  ; else scp $ID0000125_node:$ID0000125/SRR1518482.bam ./ ; fi
if [ -e $ID0000125/SRR1518482.bam.bai ]; then  /bin/ln -s $ID0000125/SRR1518482.bam.bai ./  ; else scp $ID0000125_node:$ID0000125/SRR1518482.bam.bai ./ ; fi
if [ -e $ID0000143/SRR1518944.bam ]; then  /bin/ln -s $ID0000143/SRR1518944.bam ./  ; else scp $ID0000143_node:$ID0000143/SRR1518944.bam ./ ; fi
if [ -e $ID0000135/SRR1518951.bam ]; then  /bin/ln -s $ID0000135/SRR1518951.bam ./  ; else scp $ID0000135_node:$ID0000135/SRR1518951.bam ./ ; fi
if [ -e $ID0000127/SRR3170548.bam.bai ]; then  /bin/ln -s $ID0000127/SRR3170548.bam.bai ./  ; else scp $ID0000127_node:$ID0000127/SRR3170548.bam.bai ./ ; fi
if [ -e $ID0000135/SRR1518951.bam.bai ]; then  /bin/ln -s $ID0000135/SRR1518951.bam.bai ./  ; else scp $ID0000135_node:$ID0000135/SRR1518951.bam.bai ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results-l1-j6.tar.gz SRR3150506.bam SRR3150506.bam.bai SRR1518482.bam.bai SRR1518482.bam SRR3170548.bam.bai SRR3170548.bam SRR1518969.bam.bai SRR1518969.bam SRR3177888.bam.bai SRR3177888.bam SRR2009780.bam.bai SRR2009780.bam SRR1518951.bam SRR1518951.bam.bai SRR1518933.bam.bai SRR1518933.bam SRR1518447.bam.bai SRR1518447.bam SRR1518481.bam SRR1518481.bam.bai SRR1518944.bam.bai SRR1518944.bam SRR3150357.bam.bai SRR3150357.bam 
duration=$(( $(date +%s%3N) - start ))
echo ID0000391,merge,$duration,$start,$node5 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
