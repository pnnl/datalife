tmp=$(scontrol show hostnames $SLURM_JOB_NODELIST)
cnt=0
for j in $tmp
do
    echo export node$cnt=$j
    cnt=$((cnt + 1))
done


