LD_PRELOAD=$DATALIFE_PATH/datalife/build/src/client/libclient.so  $PATH_to_1000genome-workflow/bin/individuals.py ALL.chr1.250000.vcf 1 2 3 30 

LD_PRELOAD=$DATALIFE_PATH/datalife/build/src/client/libclient.so  $PATH_to_1000genome-workflow/bin/individuals.py ALL.chr1.250000.vcf 1 29 30 30 

LD_PRELOAD=$DATALIFE_PATH/datalife/build/src/client/libclient.so  $PATH_to_1000genome-workflow/bin/individuals_merge.py 1 chr1n-1-2.tar.gz chr1n-2-3.tar.gz \dots chr1n-29-30.tar.gz 

LD_PRELOAD=$DATALIFE_PATH/datalife/build/src/client/libclient.so  $PATH_to_1000genome-workflow/bin/sifting.py ALL.chr1.phase3_shapeit2_mvncall_integrated_v5.20130502.sites.annotation.vcf 1

LD_PRELOAD=$DATALIFE_PATH/datalife/build/src/client/libclient.so  $PATH_to_1000genome-workflow/bin/mutation_overlap.py -c 1 -pop SAS 

LD_PRELOAD=$DATALIFE_PATH/datalife/build/src/client/libclient.so  $PATH_to_1000genome-workflow/bin/frequency.py -c 1 -pop SAS

