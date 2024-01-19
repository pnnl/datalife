mkdir -p $NODE_LOCAL/merge_ID0000402
cd $NODE_LOCAL/merge_ID0000402
if [ -e $ID0000392/results-l1-j7.tar.gz ]; then  /bin/ln -s $ID0000392/results-l1-j7.tar.gz ./  ; else scp $ID0000392_node:$ID0000392/results-l1-j7.tar.gz ./ ; fi
if [ -e $ID0000394/results-l1-j9.tar.gz ]; then  /bin/ln -s $ID0000394/results-l1-j9.tar.gz ./  ; else scp $ID0000394_node:$ID0000394/results-l1-j9.tar.gz ./ ; fi
if [ -e $ID0000386/results-l1-j1.tar.gz ]; then  /bin/ln -s $ID0000386/results-l1-j1.tar.gz ./  ; else scp $ID0000386_node:$ID0000386/results-l1-j1.tar.gz ./ ; fi
if [ -e $ID0000388/results-l1-j3.tar.gz ]; then  /bin/ln -s $ID0000388/results-l1-j3.tar.gz ./  ; else scp $ID0000388_node:$ID0000388/results-l1-j3.tar.gz ./ ; fi
if [ -e $ID0000390/results-l1-j5.tar.gz ]; then  /bin/ln -s $ID0000390/results-l1-j5.tar.gz ./  ; else scp $ID0000390_node:$ID0000390/results-l1-j5.tar.gz ./ ; fi
if [ -e $ID0000396/results-l1-j11.tar.gz ]; then  /bin/ln -s $ID0000396/results-l1-j11.tar.gz ./  ; else scp $ID0000396_node:$ID0000396/results-l1-j11.tar.gz ./ ; fi
if [ -e $ID0000397/results-l1-j12.tar.gz ]; then  /bin/ln -s $ID0000397/results-l1-j12.tar.gz ./  ; else scp $ID0000397_node:$ID0000397/results-l1-j12.tar.gz ./ ; fi
if [ -e $ID0000387/results-l1-j2.tar.gz ]; then  /bin/ln -s $ID0000387/results-l1-j2.tar.gz ./  ; else scp $ID0000387_node:$ID0000387/results-l1-j2.tar.gz ./ ; fi
if [ -e $ID0000391/results-l1-j6.tar.gz ]; then  /bin/ln -s $ID0000391/results-l1-j6.tar.gz ./  ; else scp $ID0000391_node:$ID0000391/results-l1-j6.tar.gz ./ ; fi
if [ -e $ID0000389/results-l1-j4.tar.gz ]; then  /bin/ln -s $ID0000389/results-l1-j4.tar.gz ./  ; else scp $ID0000389_node:$ID0000389/results-l1-j4.tar.gz ./ ; fi
if [ -e $ID0000393/results-l1-j8.tar.gz ]; then  /bin/ln -s $ID0000393/results-l1-j8.tar.gz ./  ; else scp $ID0000393_node:$ID0000393/results-l1-j8.tar.gz ./ ; fi
if [ -e $ID0000395/results-l1-j10.tar.gz ]; then  /bin/ln -s $ID0000395/results-l1-j10.tar.gz ./  ; else scp $ID0000395_node:$ID0000395/results-l1-j10.tar.gz ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results-l2-j1.tar.gz results-l1-j1.tar.gz results-l1-j2.tar.gz results-l1-j3.tar.gz results-l1-j4.tar.gz results-l1-j5.tar.gz results-l1-j6.tar.gz results-l1-j7.tar.gz results-l1-j8.tar.gz results-l1-j9.tar.gz results-l1-j10.tar.gz results-l1-j11.tar.gz results-l1-j12.tar.gz 
duration=$(( $(date +%s%3N) - start ))
echo ID0000402,merge,$duration,$start,$node0 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
