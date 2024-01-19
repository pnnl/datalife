mkdir -p $NODE_LOCAL/merge_ID0000387
cd $NODE_LOCAL/merge_ID0000387
if [ -e $ID0000029/SRR3177788.bam.bai ]; then  /bin/ln -s $ID0000029/SRR3177788.bam.bai ./  ; else scp $ID0000029_node:$ID0000029/SRR3177788.bam.bai ./ ; fi
if [ -e $ID0000047/SRR1918772.bam.bai ]; then  /bin/ln -s $ID0000047/SRR1918772.bam.bai ./  ; else scp $ID0000047_node:$ID0000047/SRR1918772.bam.bai ./ ; fi
if [ -e $ID0000045/SRR3177870.bam ]; then  /bin/ln -s $ID0000045/SRR3177870.bam ./  ; else scp $ID0000045_node:$ID0000045/SRR3177870.bam ./ ; fi
if [ -e $ID0000049/SRR3177941.bam.bai ]; then  /bin/ln -s $ID0000049/SRR3177941.bam.bai ./  ; else scp $ID0000049_node:$ID0000049/SRR3177941.bam.bai ./ ; fi
if [ -e $ID0000043/SRR1518572.bam.bai ]; then  /bin/ln -s $ID0000043/SRR1518572.bam.bai ./  ; else scp $ID0000043_node:$ID0000043/SRR1518572.bam.bai ./ ; fi
if [ -e $ID0000047/SRR1918772.bam ]; then  /bin/ln -s $ID0000047/SRR1918772.bam ./  ; else scp $ID0000047_node:$ID0000047/SRR1918772.bam ./ ; fi
if [ -e $ID0000041/SRR3150455.bam ]; then  /bin/ln -s $ID0000041/SRR3150455.bam ./  ; else scp $ID0000041_node:$ID0000041/SRR3150455.bam ./ ; fi
if [ -e $ID0000033/SRR3177971.bam ]; then  /bin/ln -s $ID0000033/SRR3177971.bam ./  ; else scp $ID0000033_node:$ID0000033/SRR3177971.bam ./ ; fi
if [ -e $ID0000031/SRR1518455.bam ]; then  /bin/ln -s $ID0000031/SRR1518455.bam ./  ; else scp $ID0000031_node:$ID0000031/SRR1518455.bam ./ ; fi
if [ -e $ID0000027/SRR1919990.bam.bai ]; then  /bin/ln -s $ID0000027/SRR1919990.bam.bai ./  ; else scp $ID0000027_node:$ID0000027/SRR1919990.bam.bai ./ ; fi
if [ -e $ID0000035/SRR1918768.bam.bai ]; then  /bin/ln -s $ID0000035/SRR1918768.bam.bai ./  ; else scp $ID0000035_node:$ID0000035/SRR1918768.bam.bai ./ ; fi
if [ -e $ID0000039/SRR1922808.bam ]; then  /bin/ln -s $ID0000039/SRR1922808.bam ./  ; else scp $ID0000039_node:$ID0000039/SRR1922808.bam ./ ; fi
if [ -e $ID0000043/SRR1518572.bam ]; then  /bin/ln -s $ID0000043/SRR1518572.bam ./  ; else scp $ID0000043_node:$ID0000043/SRR1518572.bam ./ ; fi
if [ -e $ID0000031/SRR1518455.bam.bai ]; then  /bin/ln -s $ID0000031/SRR1518455.bam.bai ./  ; else scp $ID0000031_node:$ID0000031/SRR1518455.bam.bai ./ ; fi
if [ -e $ID0000027/SRR1919990.bam ]; then  /bin/ln -s $ID0000027/SRR1919990.bam ./  ; else scp $ID0000027_node:$ID0000027/SRR1919990.bam ./ ; fi
if [ -e $ID0000029/SRR3177788.bam ]; then  /bin/ln -s $ID0000029/SRR3177788.bam ./  ; else scp $ID0000029_node:$ID0000029/SRR3177788.bam ./ ; fi
if [ -e $ID0000033/SRR3177971.bam.bai ]; then  /bin/ln -s $ID0000033/SRR3177971.bam.bai ./  ; else scp $ID0000033_node:$ID0000033/SRR3177971.bam.bai ./ ; fi
if [ -e $ID0000037/SRR1763636.bam ]; then  /bin/ln -s $ID0000037/SRR1763636.bam ./  ; else scp $ID0000037_node:$ID0000037/SRR1763636.bam ./ ; fi
if [ -e $ID0000037/SRR1763636.bam.bai ]; then  /bin/ln -s $ID0000037/SRR1763636.bam.bai ./  ; else scp $ID0000037_node:$ID0000037/SRR1763636.bam.bai ./ ; fi
if [ -e $ID0000041/SRR3150455.bam.bai ]; then  /bin/ln -s $ID0000041/SRR3150455.bam.bai ./  ; else scp $ID0000041_node:$ID0000041/SRR3150455.bam.bai ./ ; fi
if [ -e $ID0000045/SRR3177870.bam.bai ]; then  /bin/ln -s $ID0000045/SRR3177870.bam.bai ./  ; else scp $ID0000045_node:$ID0000045/SRR3177870.bam.bai ./ ; fi
if [ -e $ID0000035/SRR1918768.bam ]; then  /bin/ln -s $ID0000035/SRR1918768.bam ./  ; else scp $ID0000035_node:$ID0000035/SRR1918768.bam ./ ; fi
if [ -e $ID0000049/SRR3177941.bam ]; then  /bin/ln -s $ID0000049/SRR3177941.bam ./  ; else scp $ID0000049_node:$ID0000049/SRR3177941.bam ./ ; fi
if [ -e $ID0000039/SRR1922808.bam.bai ]; then  /bin/ln -s $ID0000039/SRR1922808.bam.bai ./  ; else scp $ID0000039_node:$ID0000039/SRR1922808.bam.bai ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results-l1-j2.tar.gz SRR1919990.bam.bai SRR1919990.bam SRR3177788.bam.bai SRR3177788.bam SRR1518455.bam SRR1518455.bam.bai SRR3177971.bam SRR3177971.bam.bai SRR1918768.bam SRR1918768.bam.bai SRR1763636.bam.bai SRR1763636.bam SRR1922808.bam SRR1922808.bam.bai SRR3150455.bam SRR3150455.bam.bai SRR1518572.bam SRR1518572.bam.bai SRR3177870.bam SRR3177870.bam.bai SRR1918772.bam SRR1918772.bam.bai SRR3177941.bam SRR3177941.bam.bai 
duration=$(( $(date +%s%3N) - start ))
echo ID0000387,merge,$duration,$start,$node1 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
