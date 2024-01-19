mkdir -p $PFS_PATH/sra-search-workflow/192sras_large/16nodes/dfman/merge_ID0000403
cd $PFS_PATH/sra-search-workflow/192sras_large/16nodes/dfman/merge_ID0000403
if [ -e $ID0000398/results-l1-j13.tar.gz ]; then  /bin/ln -s $ID0000398/results-l1-j13.tar.gz ./  ; else scp $ID0000398_node:$ID0000398/results-l1-j13.tar.gz ./ ; fi
if [ -e $ID0000401/results-l1-j16.tar.gz ]; then  /bin/ln -s $ID0000401/results-l1-j16.tar.gz ./  ; else scp $ID0000401_node:$ID0000401/results-l1-j16.tar.gz ./ ; fi
if [ -e $ID0000399/results-l1-j14.tar.gz ]; then  /bin/ln -s $ID0000399/results-l1-j14.tar.gz ./  ; else scp $ID0000399_node:$ID0000399/results-l1-j14.tar.gz ./ ; fi
if [ -e $ID0000400/results-l1-j15.tar.gz ]; then  /bin/ln -s $ID0000400/results-l1-j15.tar.gz ./  ; else scp $ID0000400_node:$ID0000400/results-l1-j15.tar.gz ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results-l2-j2.tar.gz results-l1-j13.tar.gz results-l1-j14.tar.gz results-l1-j15.tar.gz results-l1-j16.tar.gz 
duration=$(( $(date +%s%3N) - start ))
echo ID0000403,merge,$duration,$start,$node1 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/dfman/stat.log
date +%s,%H:%M:%S >> ${0}.log
