FastFlow-schedule
================================================================================

FastFlow-schedule is a tool for workflow scheduling and resource assignment based on a monitor-analyze-optimize strategy. `fastflow-schedule` allows for a SLURM job script for various scheduling techniques.

Quick start
--------------------------------------------------------------------------------

With the SRA Search workflow, fastflow runs:
...

    export NODE_LOCAL=/scratch
    export PFS_PATH=/projects/$PROJECT_NAME/
    export SLURM_PROJECT=$PROJECT_NAME
    export job_type=fastflow

    sbatch srasearch-$job_type-16nodes.slurm

