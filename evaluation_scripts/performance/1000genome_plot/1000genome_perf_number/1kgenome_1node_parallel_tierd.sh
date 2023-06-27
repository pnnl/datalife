#!/bin/bash


# !!! run the sbatch script following the examples below
# sbatch 1kgenome_parallel_shm.sbatch SHM
# sbatch 1kgenome_parallel_shm.sbatch HDD
# sbatch 1kgenome_parallel_shm.sbatch SSD
# sbatch 1kgenome_parallel_shm.sbatch NVME
set -e 
LOCAL="$1"
CURRENT_DIR=$(pwd)
export CURRENT_DIR
echo CURRENT_DIR: $CURRENT_DIR
SCRIPT_DIR=$CURRENT_DIR/1kgenome_bin

if [ "$LOCAL" == "SHM" ]; then
    # !!! Make sure you change the LOCAL_STORE to node-local RAMDISK
    # in our case, it's under /dev/shm/
    echo "Running on ramdisk"
    export LOCAL_STORE=/dev/shm/$USER
elif [ "$LOCAL" == "HDD" ]; then
    echo "Running on HDD"
    export LOCAL_STORE=/mnt/hdd1/$USER
elif [ "$LOCAL" == "SSD" ]; then
    echo "Running on SSD"
    export LOCAL_STORE=/mnt/ssd1/$USER
elif [ "$LOCAL" == "NVME" ]; then
    echo "Running on NVME"
    export LOCAL_STORE=/mnt/nvme1/$USER
else
    echo "Invalid storage disk choice ..."
    exit 1;
fi

mkdir -p $LOCAL_STORE



CHROMOSOMES=10
NUM_NODES=1

ITERATION=$(( $CHROMOSOMES / $NUM_NODES -1 ))


PROBLEM_SIZE=300 # the maximum number of tasks within a stage

NODE_NAMES="localhost"
list=()
while read -ra tmp; do
    list+=("${tmp[@]}")
done <<< "$NODE_NAMES"


LOCAL_CLEANUP () {

    echo "Cleaning up local data at $LOCAL_STORE ..."
    rm -fr $LOCAL_STORE/*gz
}

STAGE_INPUTS () {
    echo "Staging input data to $LOCAL_STORE ..."
    # modified to have targeted stage-in
    cp $CURRENT_DIR/columns.txt $LOCAL_STORE/ &
    cp $CURRENT_DIR/ALL.*.vcf $LOCAL_STORE/ &
    # cp $CURRENT_DIR/ALL.chr${ii}.phase3_shapeit2_mvncall_integrated_v5.20130502.sites.annotation.vcf $LOCAL_STORE/ &
    cp $CURRENT_DIR/SAS $LOCAL_STORE/ &
    cp $CURRENT_DIR/EAS $LOCAL_STORE/ &
    cp $CURRENT_DIR/GBR $LOCAL_STORE/ &
    cp $CURRENT_DIR/AMR $LOCAL_STORE/ &
    cp $CURRENT_DIR/AFR $LOCAL_STORE/ &
    cp $CURRENT_DIR/EUR $LOCAL_STORE/ &
    cp $CURRENT_DIR/ALL $LOCAL_STORE/ &
}

START_INDIVIDUALS() {

    counter=0
    CHROM_START=$1
    CHROM_END=$(($2 -1))

    t_count=1

    for i in $(seq $CHROM_START $CHROM_END)
    do
        for j in $(seq 1 29)
	do
	    # echo "$i $j"
            if [ "$counter" -lt "$PROBLEM_SIZE" ]
            then
                node_idx=$(($i % $NUM_NODES))

                let ii=i+1
                # No need to change. This is the smallest input allowed.

                echo $SCRIPT_DIR/individuals_shm.py $LOCAL_STORE/ALL.chr${ii}.250000.vcf $ii $j $((j+1)) 30 &
                $SCRIPT_DIR/individuals_shm.py $LOCAL_STORE/ALL.chr${ii}.250000.vcf $ii $j $((j+1)) 30 &

                counter=$(($counter + 1))

                t_count=$(($t_count + 1))

            fi
	done
    done
    # sleep 3
    set +x

}

START_INDIVIDUALS_MERGE() {

    CHROM_START=$1
    CHROM_END=$(($2 -1))

    for i in $(seq $CHROM_START $CHROM_END)
    do
    node_idx=$(($i % $NUM_NODES))
    running_node=${list[$node_idx]}
	let ii=i+1
	# 10 merge tasks in total
        python3 $SCRIPT_DIR/individuals_merge_shm.py $ii $LOCAL_STORE/chr${ii}n-1-2.tar.gz $LOCAL_STORE/chr${ii}n-2-3.tar.gz $LOCAL_STORE/chr${ii}n-3-4.tar.gz $LOCAL_STORE/chr${ii}n-4-5.tar.gz $LOCAL_STORE/chr${ii}n-5-6.tar.gz $LOCAL_STORE/chr${ii}n-6-7.tar.gz $LOCAL_STORE/chr${ii}n-7-8.tar.gz $LOCAL_STORE/chr${ii}n-8-9.tar.gz $LOCAL_STORE/chr${ii}n-9-10.tar.gz $LOCAL_STORE/chr${ii}n-10-11.tar.gz $LOCAL_STORE/chr${ii}n-11-12.tar.gz $LOCAL_STORE/chr${ii}n-12-13.tar.gz $LOCAL_STORE/chr${ii}n-13-14.tar.gz $LOCAL_STORE/chr${ii}n-14-15.tar.gz $LOCAL_STORE/chr${ii}n-15-16.tar.gz $LOCAL_STORE/chr${ii}n-16-17.tar.gz $LOCAL_STORE/chr${ii}n-17-18.tar.gz $LOCAL_STORE/chr${ii}n-18-19.tar.gz $LOCAL_STORE/chr${ii}n-19-20.tar.gz $LOCAL_STORE/chr${ii}n-20-21.tar.gz $LOCAL_STORE/chr${ii}n-21-22.tar.gz $LOCAL_STORE/chr${ii}n-22-23.tar.gz $LOCAL_STORE/chr${ii}n-23-24.tar.gz $LOCAL_STORE/chr${ii}n-24-25.tar.gz $LOCAL_STORE/chr${ii}n-25-26.tar.gz $LOCAL_STORE/chr${ii}n-26-27.tar.gz $LOCAL_STORE/chr${ii}n-27-28.tar.gz $LOCAL_STORE/chr${ii}n-28-29.tar.gz $LOCAL_STORE/chr${ii}n-29-30.tar.gz &
    done

}

START_SIFTING() {

    CHROM_START=$1
    CHROM_END=$(($2 -1))
    for i in $(seq $CHROM_START $CHROM_END)
    do
        node_idx=$(($i % $NUM_NODES))
        # 10 sifting tasks in total
        let ii=i+1
        python3 $SCRIPT_DIR/sifting.py $LOCAL_STORE/ALL.chr${ii}.phase3_shapeit2_mvncall_integrated_v5.20130502.sites.annotation.vcf $ii &
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
            python3 $SCRIPT_DIR/mutation_overlap_shm.py -c $ii -pop $j &
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
        for j in $FNAMES
        do
            let ii=i+1
            python3 $SCRIPT_DIR/frequency_shm.py -c $ii -pop $j &
        done
    done

}


hostname;date;
echo "Making directory at $LOCAL_STORE ..."
mkdir -p $LOCAL_STORE


LOCAL_CLEANUP

total_start_time=$(($(date +%s%N)/1000000))
cd $CURRENT_DIR

# Stage 0: Staging input to LOCAL_STORE
#

echo "Perform staging input ..."
start_time=$(($(date +%s%N)/1000000))
STAGE_INPUTS
wait
echo "data stage-in for individuals (msec) : $(($(date +%s%N)/1000000 - $start_time))"
start_time=$(($(date +%s%N)/1000000))


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
ls -l $LOCAL_STORE/* | wc -l

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
ls -l $LOCAL_STORE/* | wc -l

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
ls -l $LOCAL_STORE/* | wc -l


# LOCAL_CLEANUP

