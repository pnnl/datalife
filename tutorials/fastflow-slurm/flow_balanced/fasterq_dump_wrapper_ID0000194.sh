mkdir -p $NODE_LOCAL/fasterq_dump_wrapper_ID0000194
cd $NODE_LOCAL/fasterq_dump_wrapper_ID0000194
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/fasterq_dump_wrapper --split-files SRR1918783 
duration=$(( $(date +%s%3N) - start ))
echo ID0000194,fasterq_dump_wrapper,$duration,$start,$node8 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
