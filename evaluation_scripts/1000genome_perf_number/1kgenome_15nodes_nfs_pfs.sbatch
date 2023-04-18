#!/bin/bash  
#SBATCH --job-name="15nodes_nfs_pfs"
#SBATCH -N 15 
#SBATCH -n 300
#SBATCH --time=01:30:00

NODE_NAMES=`echo $SLURM_JOB_NODELIST|scontrol show hostnames`
SCRIPT_DIR=$PWD/1kgenome_bin
# !!! create an NFS or PFS path to hold the CURRENT directory and change CURRENT_DIR to that directory
CURRENT_DIR=/qfs/projects/oddite/lenny/hermes_scripts/1kgenome_sbatch_constance

PROBLEM_SIZE=300 # the maximum number of tasks within a stage
NUM_TASKS_PER_NODE=20 # (fixed) This is the max number of cores per node
NUM_NODES=$(((PROBLEM_SIZE+NUM_TASKS_PER_NODE-1)/NUM_TASKS_PER_NODE))
echo "PROBLEM SIZE: $PROBLEM_SIZE NUM_TASKS_PER_NODE: $NUM_TASKS_PER_NODE NUM_NODES: $NUM_NODES"

NODE_NAMES=`echo $SLURM_JOB_NODELIST|scontrol show hostnames`
list=()
while read -ra tmp; do
    list+=("${tmp[@]}")
done <<< "$NODE_NAMES"

START_INDIVIDUALS() {
    INCREMENT=1
    a=1
    b=2
    counter=0
    NNODES=$((NUM_NODES-1))
    chrk=1

    echo "nodelist: ${list[0]} ${list[1]}"

    for i in $(seq 0 $NNODES)
    do
        for j in $(seq 1 $NUM_TASKS_PER_NODE)
	do
	    echo "$i $j"
            if [ "$counter" -lt "$PROBLEM_SIZE" ] 
            then 
               echo "running node: ${list[$i]} t$i $a $b"
               # No need to change. This is the smallest input allowed.
               srun -w ${list[$i]} -n1 -N1 --exclusive $SCRIPT_DIR/individuals.py $CURRENT_DIR/ALL.chr${chrk}.250000.vcf $chrk $a $b 30 &
               if [ "$b" == 30 ]
               then
                   echo "a: $a b: $b chr: $chrk"
                   a=1
                   b=2
                   let chrk=chrk+1
               fi
               if [ "$b" == 30 ] && [ "$chrk" == 10 ]
               then
                   echo "All individuals tasks are submitted ..."
                   break 2
               fi
               a=$(($a + $INCREMENT))
               b=$(($b + $INCREMENT))
	       counter=$((counter++))       
            fi
	done
    done
    # sleep 3
}

START_INDIVIDUALS_MERGE() {
    for i in $(seq 1 10) 
    do
	# 10 merge tasks in total
        srun -w ${list[$i]} -n1 -N1 --exclusive $SCRIPT_DIR/individuals_merge.py $i chr${i}n*.tar.gz &
    done
}

START_SIFTING() {
    for i in $(seq 1 10)
    do
        # 10 sifting tasks in total
        srun -w ${list[$i]} -n1 -N1 --exclusive $SCRIPT_DIR/sifting.py ALL.chr${i}.phase3_shapeit2_mvncall_integrated_v5.20130502.sites.annotation.vcf $i &
    done
}

START_MUTATION_OVERLAP() {
    FNAMES=("SAS EAS GBR AMR AFR EUR ALL")
    for i in $(seq 1 10)
    do
        for j in $FNAMES
	do
	    srun -w ${list[$i]} -n1 -N1 --exclusive $SCRIPT_DIR/mutation_overlap.py -c $i -pop $j &
	done
    done
}

START_FREQUENCY() {
    FNAMES=("SAS EAS GBR AMR AFR EUR ALL")
    for i in $(seq 1 10)
    do
	for j in $FNAMES
	do
	    srun -w ${list[$i]} -n1 -N1 --exclusive $SCRIPT_DIR/frequency.py -c $i -pop $j &
	done
    done
}

timestamp1=$(($(date +%s%N)/1000000))
START_INDIVIDUALS
wait
timestamp2=$(($(date +%s%N)/1000000))
echo "individuals (msec): $((timestamp2 - timestamp1))"

START_INDIVIDUALS_MERGE

START_SIFTING
wait
timestamp3=$(($(date +%s%N)/1000000))
echo "individuals_merge+sifting (msec): $((timestamp3 - timestamp2))"

START_MUTATION_OVERLAP

START_FREQUENCY
wait
timestamp4=$(($(date +%s%N)/1000000))
echo "mutation+frequency (msec): $((timestamp4 - timestamp3))"
