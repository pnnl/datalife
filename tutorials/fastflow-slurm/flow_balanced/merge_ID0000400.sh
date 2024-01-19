mkdir -p $NODE_LOCAL/merge_ID0000400
cd $NODE_LOCAL/merge_ID0000400
if [ -e $ID0000349/SRR1518438.bam ]; then  /bin/ln -s $ID0000349/SRR1518438.bam ./  ; else scp $ID0000349_node:$ID0000349/SRR1518438.bam ./ ; fi
if [ -e $ID0000351/SRR1763652.bam.bai ]; then  /bin/ln -s $ID0000351/SRR1763652.bam.bai ./  ; else scp $ID0000351_node:$ID0000351/SRR1763652.bam.bai ./ ; fi
if [ -e $ID0000359/SRR1763662.bam.bai ]; then  /bin/ln -s $ID0000359/SRR1763662.bam.bai ./  ; else scp $ID0000359_node:$ID0000359/SRR1763662.bam.bai ./ ; fi
if [ -e $ID0000361/SRR1518953.bam.bai ]; then  /bin/ln -s $ID0000361/SRR1518953.bam.bai ./  ; else scp $ID0000361_node:$ID0000361/SRR1518953.bam.bai ./ ; fi
if [ -e $ID0000345/SRR1518962.bam.bai ]; then  /bin/ln -s $ID0000345/SRR1518962.bam.bai ./  ; else scp $ID0000345_node:$ID0000345/SRR1518962.bam.bai ./ ; fi
if [ -e $ID0000349/SRR1518438.bam.bai ]; then  /bin/ln -s $ID0000349/SRR1518438.bam.bai ./  ; else scp $ID0000349_node:$ID0000349/SRR1518438.bam.bai ./ ; fi
if [ -e $ID0000351/SRR1763652.bam ]; then  /bin/ln -s $ID0000351/SRR1763652.bam ./  ; else scp $ID0000351_node:$ID0000351/SRR1763652.bam ./ ; fi
if [ -e $ID0000347/SRR1518413.bam.bai ]; then  /bin/ln -s $ID0000347/SRR1518413.bam.bai ./  ; else scp $ID0000347_node:$ID0000347/SRR1518413.bam.bai ./ ; fi
if [ -e $ID0000353/SRR1518559.bam.bai ]; then  /bin/ln -s $ID0000353/SRR1518559.bam.bai ./  ; else scp $ID0000353_node:$ID0000353/SRR1518559.bam.bai ./ ; fi
if [ -e $ID0000355/SRR1518663.bam ]; then  /bin/ln -s $ID0000355/SRR1518663.bam ./  ; else scp $ID0000355_node:$ID0000355/SRR1518663.bam ./ ; fi
if [ -e $ID0000353/SRR1518559.bam ]; then  /bin/ln -s $ID0000353/SRR1518559.bam ./  ; else scp $ID0000353_node:$ID0000353/SRR1518559.bam ./ ; fi
if [ -e $ID0000359/SRR1763662.bam ]; then  /bin/ln -s $ID0000359/SRR1763662.bam ./  ; else scp $ID0000359_node:$ID0000359/SRR1763662.bam ./ ; fi
if [ -e $ID0000343/SRR1763651.bam ]; then  /bin/ln -s $ID0000343/SRR1763651.bam ./  ; else scp $ID0000343_node:$ID0000343/SRR1763651.bam ./ ; fi
if [ -e $ID0000345/SRR1518962.bam ]; then  /bin/ln -s $ID0000345/SRR1518962.bam ./  ; else scp $ID0000345_node:$ID0000345/SRR1518962.bam ./ ; fi
if [ -e $ID0000361/SRR1518953.bam ]; then  /bin/ln -s $ID0000361/SRR1518953.bam ./  ; else scp $ID0000361_node:$ID0000361/SRR1518953.bam ./ ; fi
if [ -e $ID0000341/SRR2009707.bam ]; then  /bin/ln -s $ID0000341/SRR2009707.bam ./  ; else scp $ID0000341_node:$ID0000341/SRR2009707.bam ./ ; fi
if [ -e $ID0000357/SRR1518563.bam.bai ]; then  /bin/ln -s $ID0000357/SRR1518563.bam.bai ./  ; else scp $ID0000357_node:$ID0000357/SRR1518563.bam.bai ./ ; fi
if [ -e $ID0000357/SRR1518563.bam ]; then  /bin/ln -s $ID0000357/SRR1518563.bam ./  ; else scp $ID0000357_node:$ID0000357/SRR1518563.bam ./ ; fi
if [ -e $ID0000341/SRR2009707.bam.bai ]; then  /bin/ln -s $ID0000341/SRR2009707.bam.bai ./  ; else scp $ID0000341_node:$ID0000341/SRR2009707.bam.bai ./ ; fi
if [ -e $ID0000339/SRR1518938.bam ]; then  /bin/ln -s $ID0000339/SRR1518938.bam ./  ; else scp $ID0000339_node:$ID0000339/SRR1518938.bam ./ ; fi
if [ -e $ID0000347/SRR1518413.bam ]; then  /bin/ln -s $ID0000347/SRR1518413.bam ./  ; else scp $ID0000347_node:$ID0000347/SRR1518413.bam ./ ; fi
if [ -e $ID0000343/SRR1763651.bam.bai ]; then  /bin/ln -s $ID0000343/SRR1763651.bam.bai ./  ; else scp $ID0000343_node:$ID0000343/SRR1763651.bam.bai ./ ; fi
if [ -e $ID0000355/SRR1518663.bam.bai ]; then  /bin/ln -s $ID0000355/SRR1518663.bam.bai ./  ; else scp $ID0000355_node:$ID0000355/SRR1518663.bam.bai ./ ; fi
if [ -e $ID0000339/SRR1518938.bam.bai ]; then  /bin/ln -s $ID0000339/SRR1518938.bam.bai ./  ; else scp $ID0000339_node:$ID0000339/SRR1518938.bam.bai ./ ; fi
date +%s,%H:%M:%S > ${0}.log
start=$(date +%s%3N)
$PFS_PATH/sra-search-workflow/tools/merge results-l1-j15.tar.gz SRR1518938.bam SRR1518938.bam.bai SRR2009707.bam.bai SRR2009707.bam SRR1763651.bam SRR1763651.bam.bai SRR1518962.bam SRR1518962.bam.bai SRR1518413.bam SRR1518413.bam.bai SRR1518438.bam SRR1518438.bam.bai SRR1763652.bam.bai SRR1763652.bam SRR1518559.bam.bai SRR1518559.bam SRR1518663.bam.bai SRR1518663.bam SRR1518563.bam SRR1518563.bam.bai SRR1763662.bam.bai SRR1763662.bam SRR1518953.bam SRR1518953.bam.bai 
duration=$(( $(date +%s%3N) - start ))
echo ID0000400,merge,$duration,$start,$node14 >> $PFS_PATH/sra-search-workflow/192sras_large/16nodes/flowopt_flow_balanced/stat.log
date +%s,%H:%M:%S >> ${0}.log
