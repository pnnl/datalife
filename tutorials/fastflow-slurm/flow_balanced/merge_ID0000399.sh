mkdir -p $NODE_LOCAL/merge_ID0000399
cd $NODE_LOCAL/merge_ID0000399
if [ -e $ID0000329/SRR1518935.bam.bai ]; then  /bin/ln -s $ID0000329/SRR1518935.bam.bai ./  ; else scp $ID0000329_node:$ID0000329/SRR1518935.bam.bai ./ ; fi
if [ -e $ID0000337/SRR1518669.bam ]; then  /bin/ln -s $ID0000337/SRR1518669.bam ./  ; else scp $ID0000337_node:$ID0000337/SRR1518669.bam ./ ; fi
if [ -e $ID0000315/SRR1518662.bam.bai ]; then  /bin/ln -s $ID0000315/SRR1518662.bam.bai ./  ; else scp $ID0000315_node:$ID0000315/SRR1518662.bam.bai ./ ; fi
if [ -e $ID0000315/SRR1518662.bam ]; then  /bin/ln -s $ID0000315/SRR1518662.bam ./  ; else scp $ID0000315_node:$ID0000315/SRR1518662.bam ./ ; fi
if [ -e $ID0000317/SRR1518412.bam ]; then  /bin/ln -s $ID0000317/SRR1518412.bam ./  ; else scp $ID0000317_node:$ID0000317/SRR1518412.bam ./ ; fi
if [ -e $ID0000325/SRR1518936.bam ]; then  /bin/ln -s $ID0000325/SRR1518936.bam ./  ; else scp $ID0000325_node:$ID0000325/SRR1518936.bam ./ ; fi
if [ -e $ID0000333/SRR1518409.bam.bai ]; then  /bin/ln -s $ID0000333/SRR1518409.bam.bai ./  ; else scp $ID0000333_node:$ID0000333/SRR1518409.bam.bai ./ ; fi
if [ -e $ID0000325/SRR1518936.bam.bai ]; then  /bin/ln -s $ID0000325/SRR1518936.bam.bai ./  ; else scp $ID0000325_node:$ID0000325/SRR1518936.bam.bai ./ ; fi
if [ -e $ID0000321/SRR1518417.bam.bai ]; then  /bin/ln -s $ID0000321/SRR1518417.bam.bai ./  ; else scp $ID0000321_node:$ID0000321/SRR1518417.bam.bai ./ ; fi
if [ -e $ID0000329/SRR1518935.bam ]; then  /bin/ln -s $ID0000329/SRR1518935.bam ./  ; else scp $ID0000329_node:$ID0000329/SRR1518935.bam ./ ; fi
if [ -e $ID0000323/SRR1518547.bam ]; then  /bin/ln -s $ID0000323/SRR1518547.bam ./  ; else scp $ID0000323_node:$ID0000323/SRR1518547.bam ./ ; fi
if [ -e $ID0000331/SRR1518474.bam.bai ]; then  /bin/ln -s $ID0000331/SRR1518474.bam.bai ./  ; else scp $ID0000331_node:$ID0000331/SRR1518474.bam.bai ./ ; fi
if [ -e $ID0000323/SRR1518547.bam.bai ]; then  /bin/ln -s $ID0000323/SRR1518547.bam.bai ./  ; else scp $ID0000323_node:$ID0000323/SRR1518547.bam.bai ./ ; fi
if [ -e $ID0000335/SRR1518470.bam ]; then  /bin/ln -s $ID0000335/SRR1518470.bam ./  ; else scp $ID0000335_node:$ID0000335/SRR1518470.bam ./ ; fi
if [ -e $ID0000327/SRR1518637.bam ]; then  /bin/ln -s $ID0000327/SRR1518637.bam ./  ; else scp $ID0000327_node:$ID0000327/SRR1518637.bam ./ ; fi
if [ -e $ID0000317/SRR1518412.bam.bai ]; then  /bin/ln -s $ID0000317/SRR1518412.bam.bai ./  ; else scp $ID0000317_node:$ID0000317/SRR1518412.bam.bai ./ ; fi
if [ -e $ID0000333/SRR1518409.bam ]; then  /bin/ln -s $ID0000333/SRR1518409.bam ./  ; else scp $ID0000333_node:$ID0000333/SRR1518409.bam ./ ; fi
if [ -e $ID0000337/SRR1518669.bam.bai ]; then  /bin/ln -s $ID0000337/SRR1518669.bam.bai ./  ; else scp $ID0000337_node:$ID0000337/SRR1518669.bam.bai ./ ; fi
if [ -e $ID0000331/SRR1518474.bam ]; then  /bin/ln -s $ID0000331/SRR1518474.bam ./  ; else scp $ID0000331_node:$ID0000331/SRR1518474.bam ./ ; fi
if [ -e $ID0000319/SRR1518411.bam.bai ]; then  /bin/ln -s $ID0000319/SRR1518411.bam.bai ./  ; else scp $ID0000319_node:$ID0000319/SRR1518411.bam.bai ./ ; fi
if [ -e $ID0000321/SRR1518417.bam ]; then  /bin/ln -s $ID0000321/SRR1518417.bam ./  ; else scp $ID0000321_node:$ID0000321/SRR1518417.bam ./ ; fi
if [ -e $ID0000335/SRR1518470.bam.bai ]; then  /bin/ln -s $ID0000335/SRR1518470.bam.bai ./  ; else scp $ID0000335_node:$ID0000335/SRR1518470.bam.bai ./ ; fi
if [ -e $ID0000319/SRR1518411.bam ]; then  /bin/ln -s $ID0000319/SRR1518411.bam ./  ; else scp $ID0000319_node:$ID0000319/SRR1518411.bam ./ ; fi
if [ -e $ID0000327/SRR1518637.bam.bai ]; then  /bin/ln -s $ID0000327/SRR1518637.bam.bai ./  ; else scp $ID0000327_node:$ID0000327/SRR1518637.bam.bai ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results-l1-j14.tar.gz SRR1518662.bam.bai SRR1518662.bam SRR1518412.bam.bai SRR1518412.bam SRR1518411.bam.bai SRR1518411.bam SRR1518417.bam.bai SRR1518417.bam SRR1518547.bam.bai SRR1518547.bam SRR1518936.bam SRR1518936.bam.bai SRR1518637.bam.bai SRR1518637.bam SRR1518935.bam.bai SRR1518935.bam SRR1518474.bam.bai SRR1518474.bam SRR1518409.bam SRR1518409.bam.bai SRR1518470.bam SRR1518470.bam.bai SRR1518669.bam SRR1518669.bam.bai 
duration=$(( $(date +%s%3N) - start ))
echo ID0000399,merge,$duration,$start,$node13 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
