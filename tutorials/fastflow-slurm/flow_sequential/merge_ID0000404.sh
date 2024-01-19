mkdir -p $PFS_PATH/sra-search-workflow/192sras_large/16nodes/dfman/merge_ID0000404
cd $PFS_PATH/sra-search-workflow/192sras_large/16nodes/dfman/merge_ID0000404
if [ -e $ID0000403/results-l2-j2.tar.gz ]; then  /bin/ln -s $ID0000403/results-l2-j2.tar.gz ./  ; else scp $ID0000403_node:$ID0000403/results-l2-j2.tar.gz ./ ; fi
if [ -e $ID0000402/results-l2-j1.tar.gz ]; then  /bin/ln -s $ID0000402/results-l2-j1.tar.gz ./  ; else scp $ID0000402_node:$ID0000402/results-l2-j1.tar.gz ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results.tar.gz results-l2-j1.tar.gz results-l2-j2.tar.gz 
duration=$(( $(date +%s%3N) - start ))
echo ID0000404,merge,$duration,$start,$node2 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/dfman/stat.log
date +%s,%H:%M:%S >> ${0}.log
