# Test Scripts used for paper final evaluations
These scripts are for the variants configurations used in the final evluations. Does not included the measurements for the original pipeline (baseline case).

## Naming
bgfs - the shared storage used is BeeGFS.
nfs - the shared storage used is NFS.
shm - the node-local storage is a tmpfs.
ssd - the node-local storage is a SSD.
cosch - stage 3 (training) and stage 4 (inference) are coscheduled on the same node.

## Path Variables
FS_PATH - The default shared storage path for the experiment, either PFS or NFS
LOCAL_PATH - The default local path for the experiments, either the tmpfs at /dev/shm or the SSD at /scratch
EXPERIMENT_PATH - Intermediate and final test output path
DDMD_PATH - Default DDMD script path
MOLECULES_PATH - Default molecules script path

## Others
The original test scripts are located on NFS, for PFS tests, all data in SCRIPT_DIR and CURRENT_DIR must be setup on the PFS path before tests start.
