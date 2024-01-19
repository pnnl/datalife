mkdir -p $NODE_LOCAL/merge_ID0000395
cd $NODE_LOCAL/merge_ID0000395
if [ -e $ID0000241/SRR1518549.bam.bai ]; then  /bin/ln -s $ID0000241/SRR1518549.bam.bai ./  ; else scp $ID0000241_node:$ID0000241/SRR1518549.bam.bai ./ ; fi
if [ -e $ID0000233/SRR2010658.bam.bai ]; then  /bin/ln -s $ID0000233/SRR2010658.bam.bai ./  ; else scp $ID0000233_node:$ID0000233/SRR2010658.bam.bai ./ ; fi
if [ -e $ID0000239/SRR1763641.bam ]; then  /bin/ln -s $ID0000239/SRR1763641.bam ./  ; else scp $ID0000239_node:$ID0000239/SRR1763641.bam ./ ; fi
if [ -e $ID0000231/SRR1518954.bam.bai ]; then  /bin/ln -s $ID0000231/SRR1518954.bam.bai ./  ; else scp $ID0000231_node:$ID0000231/SRR1518954.bam.bai ./ ; fi
if [ -e $ID0000227/SRR1518452.bam ]; then  /bin/ln -s $ID0000227/SRR1518452.bam ./  ; else scp $ID0000227_node:$ID0000227/SRR1518452.bam ./ ; fi
if [ -e $ID0000241/SRR1518549.bam ]; then  /bin/ln -s $ID0000241/SRR1518549.bam ./  ; else scp $ID0000241_node:$ID0000241/SRR1518549.bam ./ ; fi
if [ -e $ID0000221/SRR1518649.bam ]; then  /bin/ln -s $ID0000221/SRR1518649.bam ./  ; else scp $ID0000221_node:$ID0000221/SRR1518649.bam ./ ; fi
if [ -e $ID0000221/SRR1518649.bam.bai ]; then  /bin/ln -s $ID0000221/SRR1518649.bam.bai ./  ; else scp $ID0000221_node:$ID0000221/SRR1518649.bam.bai ./ ; fi
if [ -e $ID0000231/SRR1518954.bam ]; then  /bin/ln -s $ID0000231/SRR1518954.bam ./  ; else scp $ID0000231_node:$ID0000231/SRR1518954.bam ./ ; fi
if [ -e $ID0000237/SRR1518920.bam ]; then  /bin/ln -s $ID0000237/SRR1518920.bam ./  ; else scp $ID0000237_node:$ID0000237/SRR1518920.bam ./ ; fi
if [ -e $ID0000225/SRR1518554.bam.bai ]; then  /bin/ln -s $ID0000225/SRR1518554.bam.bai ./  ; else scp $ID0000225_node:$ID0000225/SRR1518554.bam.bai ./ ; fi
if [ -e $ID0000235/SRR1518585.bam ]; then  /bin/ln -s $ID0000235/SRR1518585.bam ./  ; else scp $ID0000235_node:$ID0000235/SRR1518585.bam ./ ; fi
if [ -e $ID0000219/SRR1763648.bam ]; then  /bin/ln -s $ID0000219/SRR1763648.bam ./  ; else scp $ID0000219_node:$ID0000219/SRR1763648.bam ./ ; fi
if [ -e $ID0000225/SRR1518554.bam ]; then  /bin/ln -s $ID0000225/SRR1518554.bam ./  ; else scp $ID0000225_node:$ID0000225/SRR1518554.bam ./ ; fi
if [ -e $ID0000223/SRR1518407.bam ]; then  /bin/ln -s $ID0000223/SRR1518407.bam ./  ; else scp $ID0000223_node:$ID0000223/SRR1518407.bam ./ ; fi
if [ -e $ID0000233/SRR2010658.bam ]; then  /bin/ln -s $ID0000233/SRR2010658.bam ./  ; else scp $ID0000233_node:$ID0000233/SRR2010658.bam ./ ; fi
if [ -e $ID0000227/SRR1518452.bam.bai ]; then  /bin/ln -s $ID0000227/SRR1518452.bam.bai ./  ; else scp $ID0000227_node:$ID0000227/SRR1518452.bam.bai ./ ; fi
if [ -e $ID0000229/SRR1763671.bam.bai ]; then  /bin/ln -s $ID0000229/SRR1763671.bam.bai ./  ; else scp $ID0000229_node:$ID0000229/SRR1763671.bam.bai ./ ; fi
if [ -e $ID0000237/SRR1518920.bam.bai ]; then  /bin/ln -s $ID0000237/SRR1518920.bam.bai ./  ; else scp $ID0000237_node:$ID0000237/SRR1518920.bam.bai ./ ; fi
if [ -e $ID0000229/SRR1763671.bam ]; then  /bin/ln -s $ID0000229/SRR1763671.bam ./  ; else scp $ID0000229_node:$ID0000229/SRR1763671.bam ./ ; fi
if [ -e $ID0000235/SRR1518585.bam.bai ]; then  /bin/ln -s $ID0000235/SRR1518585.bam.bai ./  ; else scp $ID0000235_node:$ID0000235/SRR1518585.bam.bai ./ ; fi
if [ -e $ID0000239/SRR1763641.bam.bai ]; then  /bin/ln -s $ID0000239/SRR1763641.bam.bai ./  ; else scp $ID0000239_node:$ID0000239/SRR1763641.bam.bai ./ ; fi
if [ -e $ID0000219/SRR1763648.bam.bai ]; then  /bin/ln -s $ID0000219/SRR1763648.bam.bai ./  ; else scp $ID0000219_node:$ID0000219/SRR1763648.bam.bai ./ ; fi
if [ -e $ID0000223/SRR1518407.bam.bai ]; then  /bin/ln -s $ID0000223/SRR1518407.bam.bai ./  ; else scp $ID0000223_node:$ID0000223/SRR1518407.bam.bai ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results-l1-j10.tar.gz SRR1763648.bam.bai SRR1763648.bam SRR1518649.bam SRR1518649.bam.bai SRR1518407.bam SRR1518407.bam.bai SRR1518554.bam SRR1518554.bam.bai SRR1518452.bam.bai SRR1518452.bam SRR1763671.bam.bai SRR1763671.bam SRR1518954.bam SRR1518954.bam.bai SRR2010658.bam SRR2010658.bam.bai SRR1518585.bam.bai SRR1518585.bam SRR1518920.bam SRR1518920.bam.bai SRR1763641.bam.bai SRR1763641.bam SRR1518549.bam.bai SRR1518549.bam 
duration=$(( $(date +%s%3N) - start ))
echo ID0000395,merge,$duration,$start,$node9 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
