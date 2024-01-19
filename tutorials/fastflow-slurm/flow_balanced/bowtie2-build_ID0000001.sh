mkdir -p $NODE_LOCAL/bowtie2-build_ID0000001
cd $NODE_LOCAL/bowtie2-build_ID0000001
/bin/ln -s $PFS_PATH/sra-search-workflow/tests/1000/crassphage.fna ./reference.fna
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
bowtie2-build reference.fna reference 
duration=$(( $(date +%s%3N) - start ))
echo ID0000001,bowtie2-build,$duration,$start,$node0 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
