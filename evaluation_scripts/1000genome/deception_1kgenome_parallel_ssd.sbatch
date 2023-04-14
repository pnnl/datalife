#!/bin/bash
#SBATCH --job-name="deception_10nodes_pfs_ssd"
#SBATCH --partition=slurm
#SBATCH --exclude=dc[119]
#SBATCH -A chess
#SBATCH -N 10
#SBATCH -n 200
#SBATCH --time=01:30:00
#SBATCH --output=R_%x.out
#SBATCH --error=R_%x.err

```
SBATCH -A oddite
SBATCH -x node32
SBATCH -x node24
```


NODE_NAMES=`echo $SLURM_JOB_NODELIST|scontrol show hostnames`

SHARE="PFS" # NFS or PFS
LOCAL="SHM" # SHM or SSD

if [ "$LOCAL" == "SHM" ]
then
    echo "Running on ramdisk"; export LOCAL_STORE=/dev/shm/$USER #shm
else
    echo "Running on SSD"; export LOCAL_STORE=/scratch/$USER
fi

if [ "$SHARE" == "NFS" ]
then
    echo "Running on NFS"
    # SCRIPT_DIR=/qfs/projects/oddite/lenny/1000genome-workflow/bin
    # CURRENT_DIR=/qfs/projects/oddite/lenny/hermes_scripts/1kgenome_sbatch #NFS
    SCRIPT_DIR=/qfs/projects/oddite/tang584/1000genome-workflow/bin
    CURRENT_DIR=/qfs/projects/oddite/tang584/1kgenome_sbatch #NFS
else
    echo "Running on PFS" # but this behaves like NFS
    PFS_DIR=/rcfs/projects/chess #/files0/oddite
    SCRIPT_DIR=$PFS_DIR/$USER/1000genome-workflow/bin
    CURRENT_DIR=$PFS_DIR/$USER/1kgenome_sbatch #PFS
    # mkdir -p $SCRIPT_DIR $CURRENT_DIR
    # if ! [ -s "$SCRIPT_DIR" ]; then cp -r /qfs/projects/oddite/tang584/1000genome-workflow/bin/* $SCRIPT_DIR/; fi
    # if ! [ -s "$CURRENT_DIR" ]; then cp -r /qfs/projects/oddite/tang584/1kgenome_sbatch/* $CURRENT_DIR/; fi
fi



# /files0/oddite/$USER/1kgenome_sbatch #SSD Burst Buffer

CHROMOSOMES=10
NUM_NODES=$SLURM_JOB_NUM_NODES

ITERATION=$(( $CHROMOSOMES / $NUM_NODES -1 ))


PROBLEM_SIZE=300 # the maximum number of tasks within a stage !!!need to modify as needed!!!
# the `SBATCH -N -n` needs to modify as well !!!

NUM_TASKS_PER_NODE=$(((PROBLEM_SIZE+NUM_TASKS_PER_NODE)/NUM_NODES)) # (fixed) This is the max number of cores per node
#NUM_NODES=$(((PROBLEM_SIZE+NUM_TASKS_PER_NODE-1)/NUM_TASKS_PER_NODE))
echo "PROBLEM SIZE: $PROBLEM_SIZE NUM_TASKS_PER_NODE: $NUM_TASKS_PER_NODE NUM_NODES: $NUM_NODES"

# module purge
# module load python/3.7.0 gcc/11.2.0

module purge
module load python/miniconda3.7 gcc/9.1.0
PYTHON_PATH=/share/apps/python/miniconda3.7/bin/python

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
                echo srun -w $running_node -n1 -N1 --exclusive $SCRIPT_DIR/individuals_shm.py $CURRENT_DIR/ALL.chr${ii}.250000.vcf $ii $j $((j+1)) 30 &
                srun -w $running_node -n1 -N1 --exclusive $SCRIPT_DIR/individuals_shm.py $CURRENT_DIR/ALL.chr${ii}.250000.vcf $ii $j $((j+1)) 30 &

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
        srun -w $running_node -n1 -N1 --exclusive $PYTHON_PATH $SCRIPT_DIR/individuals_merge_shm.py $ii $LOCAL_STORE/chr${ii}n-1-2.tar.gz $LOCAL_STORE/chr${ii}n-2-3.tar.gz $LOCAL_STORE/chr${ii}n-3-4.tar.gz $LOCAL_STORE/chr${ii}n-4-5.tar.gz $LOCAL_STORE/chr${ii}n-5-6.tar.gz $LOCAL_STORE/chr${ii}n-6-7.tar.gz $LOCAL_STORE/chr${ii}n-7-8.tar.gz $LOCAL_STORE/chr${ii}n-8-9.tar.gz $LOCAL_STORE/chr${ii}n-9-10.tar.gz $LOCAL_STORE/chr${ii}n-10-11.tar.gz $LOCAL_STORE/chr${ii}n-11-12.tar.gz $LOCAL_STORE/chr${ii}n-12-13.tar.gz $LOCAL_STORE/chr${ii}n-13-14.tar.gz $LOCAL_STORE/chr${ii}n-14-15.tar.gz $LOCAL_STORE/chr${ii}n-15-16.tar.gz $LOCAL_STORE/chr${ii}n-16-17.tar.gz $LOCAL_STORE/chr${ii}n-17-18.tar.gz $LOCAL_STORE/chr${ii}n-18-19.tar.gz $LOCAL_STORE/chr${ii}n-19-20.tar.gz $LOCAL_STORE/chr${ii}n-20-21.tar.gz $LOCAL_STORE/chr${ii}n-21-22.tar.gz $LOCAL_STORE/chr${ii}n-22-23.tar.gz $LOCAL_STORE/chr${ii}n-23-24.tar.gz $LOCAL_STORE/chr${ii}n-24-25.tar.gz $LOCAL_STORE/chr${ii}n-25-26.tar.gz $LOCAL_STORE/chr${ii}n-26-27.tar.gz $LOCAL_STORE/chr${ii}n-27-28.tar.gz $LOCAL_STORE/chr${ii}n-28-29.tar.gz $LOCAL_STORE/chr${ii}n-29-30.tar.gz &
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
        srun -w $running_node -n1 -N1 --exclusive $PYTHON_PATH $SCRIPT_DIR/sifting.py ALL.chr${ii}.phase3_shapeit2_mvncall_integrated_v5.20130502.sites.annotation.vcf $ii &
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
            srun -w $running_node -n1 -N1 --exclusive $PYTHON_PATH $SCRIPT_DIR/mutation_overlap_shm.py -c $ii -pop $j &
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
            srun -w $running_node -n1 -N1 --exclusive $PYTHON_PATH $SCRIPT_DIR/frequency_shm.py -c $ii -pop $j &
        done
    done

}


hostname;date;
echo "Making directory at $LOCAL_STORE ..."
srun -n$NUM_NODES -w $host_list --exclusive mkdir -p $LOCAL_STORE


LOCAL_CLEANUP


total_start_time=$(($(date +%s%N)/1000000))
cd $CURRENT_DIR
start_time=$(($(date +%s%N)/1000000))

# Stage 1 : Individuals
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
srun -n$NUM_NODES -w $host_list --exclusive ls -l $LOCAL_STORE/* &> /qfs/people/tang584/scripts/local-co-scheduling/1000genome/START_INDIVIDUALS.log &

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
srun -n$NUM_NODES -w $host_list --exclusive ls -l $LOCAL_STORE/* &> /qfs/people/tang584/scripts/local-co-scheduling/1000genome/SIFTING-INDIVIDUALS_MERGE.log &

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
srun -n$NUM_NODES -w $host_list --exclusive ls -l $LOCAL_STORE/* &> /qfs/people/tang584/scripts/local-co-scheduling/1000genome/MUTATION_OVERLAP-FREQUENCY.log


LOCAL_CLEANUP

hostname;date;
sacct -j $SLURM_JOB_ID -o jobid,submit,start,end,state
