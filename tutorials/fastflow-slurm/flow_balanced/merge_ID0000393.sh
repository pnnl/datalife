mkdir -p $NODE_LOCAL/merge_ID0000393
cd $NODE_LOCAL/merge_ID0000393
if [ -e $ID0000193/SRR1518557.bam ]; then  /bin/ln -s $ID0000193/SRR1518557.bam ./  ; else scp $ID0000193_node:$ID0000193/SRR1518557.bam ./ ; fi
if [ -e $ID0000175/SRR1518466.bam.bai ]; then  /bin/ln -s $ID0000175/SRR1518466.bam.bai ./  ; else scp $ID0000175_node:$ID0000175/SRR1518466.bam.bai ./ ; fi
if [ -e $ID0000191/SRR1518947.bam.bai ]; then  /bin/ln -s $ID0000191/SRR1518947.bam.bai ./  ; else scp $ID0000191_node:$ID0000191/SRR1518947.bam.bai ./ ; fi
if [ -e $ID0000183/SRR1763647.bam.bai ]; then  /bin/ln -s $ID0000183/SRR1763647.bam.bai ./  ; else scp $ID0000183_node:$ID0000183/SRR1763647.bam.bai ./ ; fi
if [ -e $ID0000189/SRR1518665.bam ]; then  /bin/ln -s $ID0000189/SRR1518665.bam ./  ; else scp $ID0000189_node:$ID0000189/SRR1518665.bam ./ ; fi
if [ -e $ID0000185/SRR1518449.bam ]; then  /bin/ln -s $ID0000185/SRR1518449.bam ./  ; else scp $ID0000185_node:$ID0000185/SRR1518449.bam ./ ; fi
if [ -e $ID0000189/SRR1518665.bam.bai ]; then  /bin/ln -s $ID0000189/SRR1518665.bam.bai ./  ; else scp $ID0000189_node:$ID0000189/SRR1518665.bam.bai ./ ; fi
if [ -e $ID0000179/SRR1518648.bam.bai ]; then  /bin/ln -s $ID0000179/SRR1518648.bam.bai ./  ; else scp $ID0000179_node:$ID0000179/SRR1518648.bam.bai ./ ; fi
if [ -e $ID0000193/SRR1518557.bam.bai ]; then  /bin/ln -s $ID0000193/SRR1518557.bam.bai ./  ; else scp $ID0000193_node:$ID0000193/SRR1518557.bam.bai ./ ; fi
if [ -e $ID0000177/SRR1518583.bam.bai ]; then  /bin/ln -s $ID0000177/SRR1518583.bam.bai ./  ; else scp $ID0000177_node:$ID0000177/SRR1518583.bam.bai ./ ; fi
if [ -e $ID0000185/SRR1518449.bam.bai ]; then  /bin/ln -s $ID0000185/SRR1518449.bam.bai ./  ; else scp $ID0000185_node:$ID0000185/SRR1518449.bam.bai ./ ; fi
if [ -e $ID0000181/SRR1518558.bam ]; then  /bin/ln -s $ID0000181/SRR1518558.bam ./  ; else scp $ID0000181_node:$ID0000181/SRR1518558.bam ./ ; fi
if [ -e $ID0000173/SRR1518623.bam ]; then  /bin/ln -s $ID0000173/SRR1518623.bam ./  ; else scp $ID0000173_node:$ID0000173/SRR1518623.bam ./ ; fi
if [ -e $ID0000191/SRR1518947.bam ]; then  /bin/ln -s $ID0000191/SRR1518947.bam ./  ; else scp $ID0000191_node:$ID0000191/SRR1518947.bam ./ ; fi
if [ -e $ID0000177/SRR1518583.bam ]; then  /bin/ln -s $ID0000177/SRR1518583.bam ./  ; else scp $ID0000177_node:$ID0000177/SRR1518583.bam ./ ; fi
if [ -e $ID0000173/SRR1518623.bam.bai ]; then  /bin/ln -s $ID0000173/SRR1518623.bam.bai ./  ; else scp $ID0000173_node:$ID0000173/SRR1518623.bam.bai ./ ; fi
if [ -e $ID0000187/SRR1518446.bam ]; then  /bin/ln -s $ID0000187/SRR1518446.bam ./  ; else scp $ID0000187_node:$ID0000187/SRR1518446.bam ./ ; fi
if [ -e $ID0000171/SRR1518930.bam.bai ]; then  /bin/ln -s $ID0000171/SRR1518930.bam.bai ./  ; else scp $ID0000171_node:$ID0000171/SRR1518930.bam.bai ./ ; fi
if [ -e $ID0000187/SRR1518446.bam.bai ]; then  /bin/ln -s $ID0000187/SRR1518446.bam.bai ./  ; else scp $ID0000187_node:$ID0000187/SRR1518446.bam.bai ./ ; fi
if [ -e $ID0000175/SRR1518466.bam ]; then  /bin/ln -s $ID0000175/SRR1518466.bam ./  ; else scp $ID0000175_node:$ID0000175/SRR1518466.bam ./ ; fi
if [ -e $ID0000181/SRR1518558.bam.bai ]; then  /bin/ln -s $ID0000181/SRR1518558.bam.bai ./  ; else scp $ID0000181_node:$ID0000181/SRR1518558.bam.bai ./ ; fi
if [ -e $ID0000171/SRR1518930.bam ]; then  /bin/ln -s $ID0000171/SRR1518930.bam ./  ; else scp $ID0000171_node:$ID0000171/SRR1518930.bam ./ ; fi
if [ -e $ID0000183/SRR1763647.bam ]; then  /bin/ln -s $ID0000183/SRR1763647.bam ./  ; else scp $ID0000183_node:$ID0000183/SRR1763647.bam ./ ; fi
if [ -e $ID0000179/SRR1518648.bam ]; then  /bin/ln -s $ID0000179/SRR1518648.bam ./  ; else scp $ID0000179_node:$ID0000179/SRR1518648.bam ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results-l1-j8.tar.gz SRR1518930.bam.bai SRR1518930.bam SRR1518623.bam.bai SRR1518623.bam SRR1518466.bam.bai SRR1518466.bam SRR1518583.bam.bai SRR1518583.bam SRR1518648.bam.bai SRR1518648.bam SRR1518558.bam SRR1518558.bam.bai SRR1763647.bam SRR1763647.bam.bai SRR1518449.bam.bai SRR1518449.bam SRR1518446.bam.bai SRR1518446.bam SRR1518665.bam SRR1518665.bam.bai SRR1518947.bam.bai SRR1518947.bam SRR1518557.bam SRR1518557.bam.bai 
duration=$(( $(date +%s%3N) - start ))
echo ID0000393,merge,$duration,$start,$node7 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
