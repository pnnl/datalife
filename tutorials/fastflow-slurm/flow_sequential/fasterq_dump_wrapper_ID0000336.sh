mkdir -p $NODE_LOCAL/fasterq_dump_wrapper_ID0000336
cd $NODE_LOCAL/fasterq_dump_wrapper_ID0000336
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/fasterq_dump_wrapper --split-files SRR1518669 
duration=$(( $(date +%s%3N) - start ))
echo ID0000336,fasterq_dump_wrapper,$duration,$start,$node13 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/dfman/stat.log
date +%s,%H:%M:%S >> ${0}.log
