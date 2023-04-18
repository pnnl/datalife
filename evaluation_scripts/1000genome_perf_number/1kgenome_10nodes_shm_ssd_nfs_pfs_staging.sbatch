#!/bin/bash
#SBATCH --job-name="10nodes_nfs_pfs_shm_ssd_staging"
#SBATCH -N 10
#SBATCH -n 200
#SBATCH --time=01:30:00

# !!! run the sbatch script following the examples below
# sbatch 1kgenome_parallel_shm.sbatch NFS SHM 1
# sbatch 1kgenome_parallel_shm.sbatch PFS SHM 1
# sbatch 1kgenome_parallel_shm.sbatch NFS SSD 0
# sbatch 1kgenome_parallel_shm.sbatch PFS SSD 0

NODE_NAMES=`echo $SLURM_JOB_NODELIST|scontrol show hostnames`

SHARE=$1 # !!! Make sure you change it to NFS or PFS for different setup
LOCAL=$2 # !!! Make sure you change it to SHM or SSD for different setup
STAGE=$3 # !!! Make sure you change it to "0" or "1" for enabling or disabling staging

if [ "$LOCAL" == "SHM" ]
then
    # !!! Make sure you change the LOCAL_STORE to node-local RAMDISK
    # in our case, it's under /dev/shm/
    echo "Running on ramdisk"; export LOCAL_STORE=/dev/shm/$USER #shm
else
    # !!! Make sure you change the LOCAL_STORE to node-local SSD
    # in our case, it's under /scratch/
    echo "Running on SSD"; export LOCAL_STORE=/scratch/$USER #ssd
fi

if [ "$SHARE" == "NFS" ]
then
    echo "Running on NFS"
    # !!! Make sure you change the `CURRENT_DIR` to a path under NFS
    # !!! It is for this and all the *sbatch scripts and input files
    # !!! If you don't have it you should create one and move all the 
    # !!! *sbatch scripts and input files there
    # !!! Make sure you change the `SCRIPT_DIR` to a path under NFS
    # !!! It holds 1000genome binaries. You should create one if you don't have it
    CURRENT_DIR=$PWD # under NFS
    SCRIPT_DIR=$CURRENT_DIR/1kgenome_bin # under NFS
else
    # !!! Make sure you change the `CURRENT_DIR` to a path under PFS, such as BeeGFS
    # !!! It is for this and all the *sbatch scripts and input files
    # !!! If you don't have it you should create one and move all the 
    # !!! *sbatch scripts and input files there
    # !!! Make sure you change the `SCRIPT_DIR` to a path under NFS
    # !!! It holds 1000genome binaries. You should create one if you don't have it
    echo "Running on PFS" # but this behaves like NFS
    CURRENT_DIR=$PWD # under PFS
    SCRIPT_DIR=$CURRENT_DIR/1kgenome_bin # under PFS
fi

CHROMOSOMES=10
NUM_NODES=$SLURM_JOB_NUM_NODES

ITERATION=$(( $CHROMOSOMES / $NUM_NODES -1 ))


PROBLEM_SIZE=300 # the maximum number of tasks within a stage

NODE_NAMES=`echo $SLURM_JOB_NODELIST|scontrol show hostnames`
list=()
while read -ra tmp; do
    list+=("${tmp[@]}")
done <<< "$NODE_NAMES"

host_list=$(echo "$NODE_NAMES" | tr '\n' ',')
echo "host_list: $host_list"
# readarray -t host_list <<< "$NODE_NAMES"

LOCAL_CLEANUP () {
    # for node in "${host_list[@]}"
    # do
    #     srun -n1 -N1 -w $node --exclusive rm -fr $LOCAL_STORE/*gz
    # done
    echo "Cleaning up local data at $LOCAL_STORE with $host_list ..."
    srun -n$NUM_NODES -w $host_list --exclusive rm -fr $LOCAL_STORE/*gz
}

STAGE_INPUTS () {
    echo "Staging input data to $LOCAL_STORE with $host_list ..."
    # modified to have targeted stage-in
    for i in $(seq 0 9)
    do
        node_idx=$(($i % $NUM_NODES))
        running_node=${list[$node_idx]}
        let ii=i+1
        echo "staging ALL.chr${ii}.250000.vcf into node $running_node"
        srun -n1 -N1 -w $running_node --exclusive cp $CURRENT_DIR/columns.txt $LOCAL_STORE/ &
        srun -n1 -N1 -w $running_node --exclusive cp $CURRENT_DIR/ALL.chr${ii}.250000.vcf $LOCAL_STORE/ &
        srun -n1 -N1 -w $running_node --exclusive cp $CURRENT_DIR/ALL.chr${ii}.phase3_shapeit2_mvncall_integrated_v5.20130502.sites.annotation.vcf $LOCAL_STORE/ &
        srun -n1 -N1 -w $running_node --exclusive cp $CURRENT_DIR/SAS $LOCAL_STORE/ &
        srun -n1 -N1 -w $running_node --exclusive cp $CURRENT_DIR/EAS $LOCAL_STORE/ &
        srun -n1 -N1 -w $running_node --exclusive cp $CURRENT_DIR/GBR $LOCAL_STORE/ &
        srun -n1 -N1 -w $running_node --exclusive cp $CURRENT_DIR/AMR $LOCAL_STORE/ &
        srun -n1 -N1 -w $running_node --exclusive cp $CURRENT_DIR/AFR $LOCAL_STORE/ &
        srun -n1 -N1 -w $running_node --exclusive cp $CURRENT_DIR/EUR $LOCAL_STORE/ &
        srun -n1 -N1 -w $running_node --exclusive cp $CURRENT_DIR/ALL $LOCAL_STORE/ &
        # srun -n1 -w $running_node --exclusive echo $(ls -l $LOCAL_STORE/*.vcf)
    done

    # srun -n$NUM_NODES -w $host_list --exclusive cp $CURRENT_DIR/ALL.chr*.250000.vcf $LOCAL_STORE/ &
}

START_INDIVIDUALS() {
    # set -x
    # rm -fr $LOCAL_STORE/chr*tar.gz
    # start_time_indiv=$SECONDS

    counter=0
    # NNODES=$((NUM_NODES-1))
    CHROM_START=$1
    CHROM_END=$(($2 -1))
    # NNODES=$(($1-1))
    # echo "running nodes $(seq $CHROM_START $CHROM_END)"
    # chrk=1
    t_count=1

    # echo "nodelist: ${list[0]} ${list[1]}"

    for i in $(seq $CHROM_START $CHROM_END)
    do
        for j in $(seq 1 29)
	do
	    # echo "$i $j"
            if [ "$counter" -lt "$PROBLEM_SIZE" ]
            then
                node_idx=$(($i % $NUM_NODES))
                running_node=${list[$node_idx]}
                # echo "running node: $running_node t$i"
                let ii=i+1
                # No need to change. This is the smallest input allowed.
                if [ "$STAGE" == "0" ]
 		then
                    echo srun -w $running_node -n1 -N1 --exclusive $SCRIPT_DIR/individuals_shm.py $CURRENT_DIR/ALL.chr${ii}.250000.vcf $ii $j $((j+1)) 30 &
                    srun -w $running_node -n1 -N1 --exclusive $SCRIPT_DIR/individuals_shm.py $CURRENT_DIR/ALL.chr${ii}.250000.vcf $ii $j $((j+1)) 30 &
		else
                    echo srun -w $running_node -n1 -N1 --exclusive $SCRIPT_DIR/individuals_shm.py $LOCAL_STORE/ALL.chr${ii}.250000.vcf $ii $j $((j+1)) 30 &
                    srun -w $running_node -n1 -N1 --exclusive $SCRIPT_DIR/individuals_shm.py $LOCAL_STORE/ALL.chr${ii}.250000.vcf $ii $j $((j+1)) 30 &
		fi
                counter=$(($counter + 1))
                # echo "counter: $counter"

                t_count=$(($t_count + 1))
                # echo "t_count: $t_count"
                # if [[ $t_count == 20 ]]
                # then
                #     echo "finished launching $t_count tasks, now wait"
                #     t_count=1
                #     wait
                # fi

            fi
	done
    done
    # sleep 3
    set +x

}

START_INDIVIDUALS_MERGE() {

    CHROM_START=$1
    CHROM_END=$(($2 -1))
    # NNODES=$((NUM_NODES-1))

    # for i in $(seq 0 $NNODES)
    for i in $(seq $CHROM_START $CHROM_END)
    do
    node_idx=$(($i % $NUM_NODES))
    running_node=${list[$node_idx]}
	let ii=i+1
	# 10 merge tasks in total
        srun -w $running_node -n1 -N1 --exclusive python3 $SCRIPT_DIR/individuals_merge_shm.py $ii $LOCAL_STORE/chr${ii}n-1-2.tar.gz $LOCAL_STORE/chr${ii}n-2-3.tar.gz $LOCAL_STORE/chr${ii}n-3-4.tar.gz $LOCAL_STORE/chr${ii}n-4-5.tar.gz $LOCAL_STORE/chr${ii}n-5-6.tar.gz $LOCAL_STORE/chr${ii}n-6-7.tar.gz $LOCAL_STORE/chr${ii}n-7-8.tar.gz $LOCAL_STORE/chr${ii}n-8-9.tar.gz $LOCAL_STORE/chr${ii}n-9-10.tar.gz $LOCAL_STORE/chr${ii}n-10-11.tar.gz $LOCAL_STORE/chr${ii}n-11-12.tar.gz $LOCAL_STORE/chr${ii}n-12-13.tar.gz $LOCAL_STORE/chr${ii}n-13-14.tar.gz $LOCAL_STORE/chr${ii}n-14-15.tar.gz $LOCAL_STORE/chr${ii}n-15-16.tar.gz $LOCAL_STORE/chr${ii}n-16-17.tar.gz $LOCAL_STORE/chr${ii}n-17-18.tar.gz $LOCAL_STORE/chr${ii}n-18-19.tar.gz $LOCAL_STORE/chr${ii}n-19-20.tar.gz $LOCAL_STORE/chr${ii}n-20-21.tar.gz $LOCAL_STORE/chr${ii}n-21-22.tar.gz $LOCAL_STORE/chr${ii}n-22-23.tar.gz $LOCAL_STORE/chr${ii}n-23-24.tar.gz $LOCAL_STORE/chr${ii}n-24-25.tar.gz $LOCAL_STORE/chr${ii}n-25-26.tar.gz $LOCAL_STORE/chr${ii}n-26-27.tar.gz $LOCAL_STORE/chr${ii}n-27-28.tar.gz $LOCAL_STORE/chr${ii}n-28-29.tar.gz $LOCAL_STORE/chr${ii}n-29-30.tar.gz &
    done

}

START_SIFTING() {
    # NNODES=$((NUM_NODES-1))
    # for i in $(seq 0 $NNODES)
    CHROM_START=$1
    CHROM_END=$(($2 -1))
    for i in $(seq $CHROM_START $CHROM_END)
    do
    node_idx=$(($i % $NUM_NODES))
    running_node=${list[$node_idx]}
        # 10 sifting tasks in total
	let ii=i+1
        if [ "$STAGE" == "0" ]
 	then
            srun -w $running_node -n1 -N1 --exclusive python3 $SCRIPT_DIR/sifting.py ALL.chr${ii}.phase3_shapeit2_mvncall_integrated_v5.20130502.sites.annotation.vcf $ii &
        else
            srun -w $running_node -n1 -N1 --exclusive python3 $SCRIPT_DIR/sifting.py $LOCAL_STORE/ALL.chr${ii}.phase3_shapeit2_mvncall_integrated_v5.20130502.sites.annotation.vcf $ii &
        fi
    done

}

START_MUTATION_OVERLAP() {
    # NNODES=$((NUM_NODES-1))
    # start_time_mutat=$SECONDS
    FNAMES=("SAS EAS GBR AMR AFR EUR ALL")
    # for i in $(seq 0 $NNODES)
    # do
    CHROM_START=$1
    CHROM_END=$(($2 -1))
    for i in $(seq $CHROM_START $CHROM_END)
    do
        node_idx=$(($i % $NUM_NODES))
        running_node=${list[$node_idx]}
        for j in $FNAMES
        do
            let ii=i+1
            srun -w $running_node -n1 -N1 --exclusive python3 $SCRIPT_DIR/mutation_overlap_shm.py -c $ii -pop $j &
        done
    done

}

START_FREQUENCY() {

    FNAMES=("SAS EAS GBR AMR AFR EUR ALL")
    CHROM_START=$1
    CHROM_END=$(($2 -1))
    for i in $(seq $CHROM_START $CHROM_END)
    do
        node_idx=$(($i % $NUM_NODES))
        running_node=${list[$node_idx]}
        for j in $FNAMES
        do
            let ii=i+1
            srun -w $running_node -n1 -N1 --exclusive python3 $SCRIPT_DIR/frequency_shm.py -c $ii -pop $j &
        done
    done

}


hostname;date;
echo "Making directory at $LOCAL_STORE ..."
srun -n$NUM_NODES -w $host_list --exclusive mkdir -p $LOCAL_STORE


LOCAL_CLEANUP

total_start_time=$(($(date +%s%N)/1000000))
cd $CURRENT_DIR

# Stage 0: Staging input to LOCAL_STORE
#
if [ "$STAGE" == "1" ]
then
    echo "Perform staging input ..."
    start_time=$(($(date +%s%N)/1000000))
    STAGE_INPUTS
    wait
    echo "data stage-in for individuals (msec) : $(($(date +%s%N)/1000000 - $start_time))"
    start_time=$(($(date +%s%N)/1000000))
fi

# Stage 1 : Individuals
start_time=$(($(date +%s%N)/1000000))
for i in $(seq 0 $ITERATION)
do
    START_CHROMOSOME=$(( $i * $NUM_NODES ))
    END_CHROMOSOME=$(( $i * $NUM_NODES + $NUM_NODES ))
    echo "individuals CHROMOSOME from $START_CHROMOSOME to $END_CHROMOSOME"
    START_INDIVIDUALS $START_CHROMOSOME $END_CHROMOSOME
    wait
done
wait
echo "individuals (msec) : $(($(date +%s%N)/1000000 - $start_time))"

# check output
echo "Checking output after START_INDIVIDUALS ----------------------------------"
srun -n$NUM_NODES -w $host_list --exclusive ls -l $LOCAL_STORE/* &> $CURRENT_DIR/START_INDIVIDUALS.log &

# Stage 2 : Individuals merge + Sifting
start_time=$(($(date +%s%N)/1000000))
for i in $(seq 0 $ITERATION)
do
    START_CHROMOSOME=$(( $i * $NUM_NODES ))
    END_CHROMOSOME=$(( $i * $NUM_NODES + $NUM_NODES ))
    echo "individuals_merge+sifting from $START_CHROMOSOME to $END_CHROMOSOME"
    START_SIFTING $START_CHROMOSOME $END_CHROMOSOME
    START_INDIVIDUALS_MERGE $START_CHROMOSOME $END_CHROMOSOME
    wait
done
wait
echo "individuals_merge+sifting (msec) : $(($(date +%s%N)/1000000 - $start_time))"

# check output
echo "Checking output after START_SIFTING & START_INDIVIDUALS_MERGE ----------------"
srun -n$NUM_NODES -w $host_list --exclusive ls -l $LOCAL_STORE/* &> $CURRENT_DIR/SIFTING-INDIVIDUALS_MERGE.log &

# Stage 3 : Mutation overlap + Frequency
start_time=$(($(date +%s%N)/1000000))
for i in $(seq 0 $ITERATION)
do
    START_CHROMOSOME=$(( $i * $NUM_NODES ))
    END_CHROMOSOME=$(( $i * $NUM_NODES + $NUM_NODES ))
    echo "individuals_merge+sifting from $START_CHROMOSOME to $END_CHROMOSOME"

    START_MUTATION_OVERLAP $START_CHROMOSOME $END_CHROMOSOME
    START_FREQUENCY $START_CHROMOSOME $END_CHROMOSOME
    wait
done
echo "mutation+frequency (msec) : $(($(date +%s%N)/1000000 - $start_time))"

total_duration=$(( $(date +%s%N)/1000000 - $total_start_time))
echo "All done (msec) : $total_duration"

# check output
echo "Checking output after START_MUTATION_OVERLAP & START_FREQUENCY ----------------"
srun -n$NUM_NODES -w $host_list --exclusive ls -l $LOCAL_STORE/* &> $CURRENT_DIR/MUTATION_OVERLAP-FREQUENCY.log


LOCAL_CLEANUP

hostname;date;
sacct -j $SLURM_JOB_ID -o jobid,submit,start,end,state
