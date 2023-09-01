#!/bin/bash  
#SBATCH -A oddite
#SBATCH --job-name="10nodes_nfs_bfs"
#SBATCH -N 10 
#SBATCH -n 200
#SBATCH --time=01:30:00

NODE_NAMES=`echo $SLURM_JOB_NODELIST|scontrol show hostnames`
SCRIPT_DIR=$PWD/1kgenome_bin
# !!! create an NFS or BFS path to hold the CURRENT directory and change CURRENT_DIR to that directory
CURRENT_DIR=/people/guol678/projects/datalife/evaluation_scripts/1000genome_generic

PROBLEM_SIZE=300 # the maximum number of tasks within a stage
NUM_NODES=10
echo "PROBLEM SIZE: $PROBLEM_SIZE NUM_NODES: $NUM_NODES"

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
        for j in $(seq 1 29)
	do
	    echo "$i $j"
            if [ "$counter" -lt "$PROBLEM_SIZE" ] 
            then 
               echo "running node: ${list[$i]} t$i $a $b"
	       let ii=i+1
               # No need to change. This is the smallest input allowed.
               srun -w ${list[$i]} -n1 -N1 --exclusive $SCRIPT_DIR/individuals.py $CURRENT_DIR/ALL.chr${ii}.250000.vcf $ii $j $((j+1)) 30 &
	       counter=$((counter++))       
            fi
	done
    done
    # sleep 3
}

START_INDIVIDUALS_MERGE() {
    NNODES=$((NUM_NODES-1))
    for i in $(seq 0 $NNODES)
    do
	let ii=i+1
	# 10 merge tasks in total
        srun -w ${list[$i]} -n1 -N1 --exclusive $SCRIPT_DIR/individuals_merge.py $ii chr${ii}n-1-2.tar.gz chr${ii}n-2-3.tar.gz chr${ii}n-3-4.tar.gz chr${ii}n-4-5.tar.gz chr${ii}n-5-6.tar.gz chr${ii}n-6-7.tar.gz chr${ii}n-7-8.tar.gz chr${ii}n-8-9.tar.gz chr${ii}n-9-10.tar.gz chr${ii}n-10-11.tar.gz chr${ii}n-11-12.tar.gz chr${ii}n-12-13.tar.gz chr${ii}n-13-14.tar.gz chr${ii}n-14-15.tar.gz chr${ii}n-15-16.tar.gz chr${ii}n-16-17.tar.gz chr${ii}n-17-18.tar.gz chr${ii}n-18-19.tar.gz chr${ii}n-19-20.tar.gz chr${ii}n-20-21.tar.gz chr${ii}n-21-22.tar.gz chr${ii}n-22-23.tar.gz chr${ii}n-23-24.tar.gz chr${ii}n-24-25.tar.gz chr${ii}n-25-26.tar.gz chr${ii}n-26-27.tar.gz chr${ii}n-27-28.tar.gz chr${ii}n-28-29.tar.gz chr${ii}n-29-30.tar.gz & 
    done
}

START_SIFTING() {
    NNODES=$((NUM_NODES-1))
    for i in $(seq 0 $NNODES)
    do
        # 10 sifting tasks in total
	let ii=i+1
        srun -w ${list[$i]} -n1 -N1 --exclusive $SCRIPT_DIR/sifting.py ALL.chr${ii}.phase3_shapeit2_mvncall_integrated_v5.20130502.sites.annotation.vcf $ii &
    done
}

START_MUTATION_OVERLAP() {
    NNODES=$((NUM_NODES-1))
    FNAMES=("SAS EAS GBR AMR AFR EUR ALL")
    for i in $(seq 0 $NNODES)
    do
        for j in $FNAMES
	do
	    let ii=i+1
	    srun -w ${list[$i]} -n1 -N1 --exclusive $SCRIPT_DIR/mutation_overlap.py -c $ii -pop $j &
	done
    done
}

START_FREQUENCY() {
    NNODES=$((NUM_NODES-1))
    FNAMES=("SAS EAS GBR AMR AFR EUR ALL")
    for i in $(seq 0 $NNODES)
    do
	for j in $FNAMES
	do
	    let ii=i+1
	    srun -w ${list[$i]} -n1 -N1 --exclusive $SCRIPT_DIR/frequency.py -c $ii -pop $j &
	done
    done
}

LOCAL_CLEANUP
wait

cd $CURRENT_DIR
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

LOCAL_CLEANUP
wait
echo "mutation+frequency (msec): $((timestamp4 - timestamp3))"
