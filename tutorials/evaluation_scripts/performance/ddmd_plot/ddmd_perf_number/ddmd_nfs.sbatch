#!/bin/bash
#SBATCH --job-name=org_ddmd_n2t12i5_100ps_nfs1
#SBATCH --partition=dlt
#SBATCH --exclude=dlt[02]
#SBATCH --account=chess
#SBATCH --time=00:30:00
#SBATCH -N 2
#SBATCH -n 12
#SBATCH --output=./R_%x.out                                        
#SBATCH --error=./R_%x.err

# --gres=gpu:6

SKIP_OPENMM=false
SHORTENED_PIPELINE=false
MD_RUNS=12
ITER_COUNT=5 # TBD
SIM_LENGTH=0.1
SIZE=$(echo "$SIM_LENGTH * 1000" | bc)
SIZE=${SIZE%.*}
TRIAL="nfs1"
FS_PATH="NFS"

if [ "$FS_PATH" == "NFS" ]
then
    echo "Running on NFS"
    EXPERIMENT_PATH=/qfs/projects/oddite/$USER/ddmd_runs/test_${SIZE}ps_i${ITER_COUNT}_${TRIAL} #NFS
    DDMD_PATH=/people/leeh736/git/deepdrivemd #NFS
    MOLECULES_PATH=/qfs/projects/oddite/leeh736/git/molecules #NFS
else
    echo "Running on PFS"
    EXPERIMENT_PATH=/rcfs/projects/chess/$USER/ddmd_runs/test_${SIZE}ps_i${ITER_COUNT}_${TRIAL} #PFS
    DDMD_PATH=/rcfs/projects/chess/$USER/git/deepdrivemd #PFS
    MOLECULES_PATH=/rcfs/projects/chess/$USER/git/molecules #PFS
fi

mkdir -p $DDMD_PATH
mkdir -p $EXPERIMENT_PATH


GPU_PER_NODE=6
MD_START=0
NODE_COUNT=$SLURM_JOB_NUM_NODES
MD_SLICE=$(($MD_RUNS/$NODE_COUNT))
STAGE_IDX=0
STAGE_IDX_FORMAT=$(seq -f "stage%04g" $STAGE_IDX $STAGE_IDX)

NODE_NAMES=`echo $SLURM_JOB_NODELIST|scontrol show hostnames`

rm -rf $EXPERIMENT_PATH/*
ls $EXPERIMENT_PATH/* -hl

# OPENMM_PYTHON=~/.conda/envs/hm_ddmd_openmm_deception/bin/python
# PYTORCH_PYTHON=~/.conda/envs/hm_ddmd_pytorch_deception/bin/python

# module purge
# module load python/miniconda3.7 gcc/9.1.0 git/2.31.1 cmake/3.21.4 openmpi/4.1.3
# source /share/apps/python/miniconda3.7/etc/profile.d/conda.sh
# conda activate hermes_ddmd #/files0/oddite/conda/ddmd/

readarray -t NODE_ARR <<< "$NODE_NAMES"
TRAINING_NODE=${NODE_ARR[1]}
INFERENCE_NODE=${NODE_ARR[0]}
echo "TRAINING_NODE on = ${TRAINING_NODE}"
echo "INFERENCE_NODE on = ${INFERENCE_NODE}"

OPENMM () {

    task_id=$(seq -f "task%04g" $1 $1)
    gpu_idx=$(($1 % $GPU_PER_NODE))
    node_id=$2
    yaml_path=$3
    stage_name="molecular_dynamics"
    dest_path=$EXPERIMENT_PATH/${stage_name}_runs/$STAGE_IDX_FORMAT/$task_id

    if [ "$yaml_path" == "" ]
    then
        yaml_path=$DDMD_PATH/test/bba/${stage_name}_stage_test.yaml
    fi

    module load python/miniconda3.7 gcc/9.1.0
    source activate hm_ddmd_openmm_deception

    mkdir -p $dest_path
    cd $dest_path
    echo cd $dest_path

    sed -e "s/\$SIM_LENGTH/${SIM_LENGTH}/" -e "s/\$OUTPUT_PATH/${dest_path//\//\\/}/" -e "s/\$EXPERIMENT_PATH/${EXPERIMENT_PATH//\//\\/}/" -e "s/\$DDMD_PATH/${DDMD_PATH//\//\\/}/" -e "s/\$GPU_IDX/${gpu_idx}/" -e "s/\$STAGE_IDX/${STAGE_IDX}/" $yaml_path  > $dest_path/$(basename $yaml_path)
    yaml_path=$dest_path/$(basename $yaml_path)

    PYTHONPATH=$DDMD_PATH:$MOLECULES_PATH srun -w $node_id -n1 -N1 --exclusive ~/.conda/envs/hm_ddmd_openmm_deception/bin/python $DDMD_PATH/deepdrivemd/sim/openmm/run_openmm.py -c $yaml_path &> ${task_id}_${FUNCNAME[0]}.log &

    # PYTHONPATH=$DDMD_PATH:$MOLECULES_PATH srun -w $node_id -n1 -N1 --exclusive ~/.conda/envs/hm_ddmd_openmm_deception/bin/python $DDMD_PATH/deepdrivemd/sim/openmm/run_openmm.py -c $yaml_path &> ${task_id}_${FUNCNAME[0]}.log &
    #PYTHONPATH=~/git/molecules/ srun -w $node_id -N1 python /people/leeh736/git/DeepDriveMD-pipeline/deepdrivemd/sim/openmm/run_openmm.py -c $yaml_path &>> $task_id.log &
    #srun -n1 env LD_PRELOAD=~/git/tazer_forked/build.h5/src/client/libclient.so PYTHONPATH=~/git/molecules/ python /people/leeh736/git/DeepDriveMD-pipeline/deepdrivemd/sim/openmm/run_openmm.py -c /qfs/projects/oddite/leeh736/ddmd_runs/test/md_direct.yaml &> $task_id.log &
}

AGGREGATE () {

    task_id=task0000
    stage_name="aggregate"
    STAGE_IDX=$(($STAGE_IDX - 1))
    STAGE_IDX_FORMAT=$(seq -f "stage%04g" $STAGE_IDX $STAGE_IDX)
    dest_path=$EXPERIMENT_PATH/molecular_dynamics_runs/$STAGE_IDX_FORMAT/$task_id
    yaml_path=$DDMD_PATH/test/bba/${stage_name}_stage_test.yaml


    module load python/miniconda3.7 gcc/9.1.0
    source activate hm_ddmd_openmm_deception
    mkdir -p $dest_path
    cd $dest_path
    echo cd $dest_path

    sed -e "s/\$SIM_LENGTH/${SIM_LENGTH}/" -e "s/\$OUTPUT_PATH/${dest_path//\//\\/}/" -e "s/\$EXPERIMENT_PATH/${EXPERIMENT_PATH//\//\\/}/" -e "s/\$STAGE_IDX/${STAGE_IDX}/" $yaml_path  > $dest_path/$(basename $yaml_path)
    yaml_path=$dest_path/$(basename $yaml_path)

    { time PYTHONPATH=$DDMD_PATH/ ~/.conda/envs/hm_ddmd_openmm_deception/bin/python $DDMD_PATH/deepdrivemd/aggregation/basic/aggregate.py -c $yaml_path ; } &> ${task_id}_${FUNCNAME[0]}.log

    #env LD_PRELOAD=/qfs/people/leeh736/git/tazer_forked/build.h5.pread64.bluesky/src/client/libclient.so PYTHONPATH=$DDMD_PATH/ python /files0/oddite/deepdrivemd/src/deepdrivemd/aggregation/basic/aggregate.py -c /qfs/projects/oddite/leeh736/ddmd_runs/1k/agg_test.yaml &> agg_test_output.log
}

TRAINING () {

    task_id=task0000
    stage_name="machine_learning"
    dest_path=$EXPERIMENT_PATH/${stage_name}_runs/$STAGE_IDX_FORMAT/$task_id
    stage_name="training"
    yaml_path=$DDMD_PATH/test/bba/${stage_name}_stage_test.yaml


    mkdir -p $EXPERIMENT_PATH/model_selection_runs/$STAGE_IDX_FORMAT/$task_id/
    cp -p $DDMD_PATH/test/bba/stage0000_$task_id.json $EXPERIMENT_PATH/model_selection_runs/$STAGE_IDX_FORMAT/$task_id/${STAGE_IDX_FORMAT}_$task_id.json


    module load python/miniconda3.7 gcc/9.1.0
    source activate hm_ddmd_pytorch_deception

    mkdir -p $dest_path
    cd $dest_path
    echo cd $dest_path

    sed -e "s/\$SIM_LENGTH/${SIM_LENGTH}/" -e "s/\$OUTPUT_PATH/${dest_path//\//\\/}/" -e "s/\$EXPERIMENT_PATH/${EXPERIMENT_PATH//\//\\/}/" -e "s/\$STAGE_IDX/${STAGE_IDX}/" $yaml_path  > $dest_path/$(basename $yaml_path)
    yaml_path=$dest_path/$(basename $yaml_path)
    
    # PYTHONPATH=$DDMD_PATH/:$MOLECULES_PATH ~/.conda/envs/hm_ddmd_pytorch_deception/bin/python $DDMD_PATH/deepdrivemd/models/aae/train.py -c $yaml_path &> ${task_id}_${FUNCNAME[0]}.log 

   echo PYTHONPATH=$DDMD_PATH/:$MOLECULES_PATH ~/.conda/envs/hm_ddmd_pytorch_deception/bin/python $DDMD_PATH/deepdrivemd/models/aae/train.py -c $yaml_path ${task_id}_${FUNCNAME[0]}.log 
   PYTHONPATH=$DDMD_PATH/:$MOLECULES_PATH ~/.conda/envs/hm_ddmd_pytorch_deception/bin/python $DDMD_PATH/deepdrivemd/models/aae/train.py -c $yaml_path &> ${task_id}_${FUNCNAME[0]}.log
#    if [ "$SHORTENED_PIPELINE" == true ]
#    then
#        PYTHONPATH=$DDMD_PATH/:$MOLECULES_PATH srun -n1 -N1 --exclusive ~/.conda/envs/hm_ddmd_pytorch_deception/bin/python $DDMD_PATH/deepdrivemd/models/aae/train.py -c $yaml_path &> ${task_id}_${FUNCNAME[0]}.log &
#    else
#        PYTHONPATH=$DDMD_PATH/:$MOLECULES_PATH srun -n1 -N1 --exclusive ~/.conda/envs/hm_ddmd_pytorch_deception/bin/python $DDMD_PATH/deepdrivemd/models/aae/train.py -c $yaml_path &> ${task_id}_${FUNCNAME[0]}.log
#    fi

}

INFERENCE () {

    task_id=task0000
    stage_name="inference"
    dest_path=$EXPERIMENT_PATH/${stage_name}_runs/$STAGE_IDX_FORMAT/$task_id
    yaml_path=$DDMD_PATH/test/bba/${stage_name}_stage_test.yaml
    pretrained_model=$DDMD_PATH/data/bba/epoch-130-20201203-150026.pt


    # module purge
    # module load python/miniconda3.7 gcc/9.1.0 git/2.31.1 cmake/3.21.4 openmpi/4.1.3
    # source /share/apps/python/miniconda3.7/etc/profile.d/conda.sh
    module load python/miniconda3.7 gcc/9.1.0
    source activate hm_ddmd_pytorch_deception

    mkdir -p $dest_path
    cd $dest_path
    echo cd $dest_path

    echo "INFERENCE $STAGE_IDX_FORMAT $STAGE_IDX"

    mkdir -p $EXPERIMENT_PATH/agent_runs/$STAGE_IDX_FORMAT/$task_id/

    sed -e "s/\$SIM_LENGTH/${SIM_LENGTH}/" -e "s/\$OUTPUT_PATH/${dest_path//\//\\/}/" -e "s/\$EXPERIMENT_PATH/${EXPERIMENT_PATH//\//\\/}/" -e "s/\$STAGE_IDX/${STAGE_IDX}/" $yaml_path  > $dest_path/$(basename $yaml_path)
    yaml_path=$dest_path/$(basename $yaml_path)
    
    # latest model search
    model_checkpoint=$(find $EXPERIMENT_PATH/machine_learning_runs/*/*/checkpoint -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" ")
    if [ "$model_checkpoint" == "" ] && [ "$SHORTENED_PIPELINE" == true ]
    then
        model_checkpoint=$pretrained_model
    fi
    STAGE_IDX_PREV=$((STAGE_IDX - 1))
    STAGE_IDX_FORMAT_PREV=$(seq -f "stage%04g" $STAGE_IDX_PREV $STAGE_IDX_PREV)
    sed -i -e "s/\$MODEL_CHECKPOINT/${model_checkpoint//\//\\/}/"  $EXPERIMENT_PATH/model_selection_runs/$STAGE_IDX_FORMAT_PREV/task0000/${STAGE_IDX_FORMAT_PREV}_task0000.json


    OMP_NUM_THREADS=4 PYTHONPATH=$DDMD_PATH/:$MOLECULES_PATH ~/.conda/envs/hm_ddmd_pytorch_deception/bin/python $DDMD_PATH/deepdrivemd/agents/lof/lof.py -c $yaml_path &> ${task_id}_${FUNCNAME[0]}.log 

}
#

# Python environment on Bluesky
# module load python/miniconda3.7 gcc/9.1.0
# source activate openmm

# set -x

STAGE_UPDATE() {

    STAGE_IDX=$(($STAGE_IDX + 1))
    tmp=$(seq -f "stage%04g" $STAGE_IDX $STAGE_IDX)
    echo $tmp
}

total_start_time=$(($(date +%s%N)/1000000))

for iter in $(seq $ITER_COUNT)
do

    # STAGE 1: OpenMM
    start_time=$(($(date +%s%N)/1000000))
    for node in $NODE_NAMES
    do
        while [ $MD_SLICE -gt 0 ] && [ $MD_START -lt $MD_RUNS ]
        do
            echo $node
            OPENMM $MD_START $node # --oversubscribe
            # srun -n1 -N1 --exclusive -w $node $( OPENMM $MD_START $node ) &
            MD_START=$(($MD_START + 1))
            MD_SLICE=$(($MD_SLICE - 1))
        done
        MD_SLICE=$(($MD_RUNS/$NODE_COUNT))
    done
    wait

    MD_START=0

    duration=$(( $(date +%s%N)/1000000 - $start_time))
    echo "OpenMM done... $duration milliseconds elapsed."

    STAGE_IDX_FORMAT="$(STAGE_UPDATE)"
    STAGE_IDX=$((STAGE_IDX + 1))
    echo $STAGE_IDX_FORMAT

    # STAGE 2: Aggregate       
    if [ "$SHORTENED_PIPELINE" != true ]
    then
        start_time=$(($(date +%s%N)/1000000))
        srun -N1 $( AGGREGATE )
        wait 
        duration=$(( $(date +%s%N)/1000000 - $start_time))
        echo "Aggregate done... $duration milliseconds elapsed."
    else
        echo "No AGGREGATE, SHORTENED_PIPELINE = $SHORTENED_PIPELINE..."
    fi
    
    
    STAGE_IDX_FORMAT="$(STAGE_UPDATE)"
    STAGE_IDX=$((STAGE_IDX + 1))
    echo $STAGE_IDX_FORMAT

    # STAGE 3: Training
    start_time=$(($(date +%s%N)/1000000))
    # TRAINING
    if [ "$SHORTENED_PIPELINE" != true ]
    then
        srun -n1 -N1 --exclusive $( TRAINING )
        wait
        duration=$(( $(date +%s%N)/1000000 - $start_time))
        echo "Training done... $duration milliseconds elapsed."
    else
        srun  -n1 -N1 --exclusive $( TRAINING ) &
        echo "Training not waited, SHORTENED_PIPELINE = $SHORTENED_PIPELINE..."
    fi

    STAGE_IDX_FORMAT="$(STAGE_UPDATE)"
    STAGE_IDX=$((STAGE_IDX + 1))
    echo $STAGE_IDX_FORMAT $STAGE_IDX

    # STAGE 4: Inference
    start_time=$(($(date +%s%N)/1000000))
    srun $( INFERENCE )
    wait
    duration=$(( $(date +%s%N)/1000000 - $start_time))
    echo "Inference done... $duration milliseconds elapsed."

    STAGE_IDX_FORMAT="$(STAGE_UPDATE)"
    STAGE_IDX=$((STAGE_IDX + 1))
    echo $STAGE_IDX_FORMAT


done

# wait

total_duration=$(( $(date +%s%N)/1000000 - $total_start_time))
echo "All done... $total_duration milliseconds elapsed."

ls $EXPERIMENT_PATH/*/*/* -hl

hostname;date;
sacct -j $SLURM_JOB_ID -o jobid,submit,start,end,state
