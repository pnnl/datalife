# Test Scripts used for paper final evaluations
These scripts are for the variants configurations used in the final evluations. \
The scripts are usable for 10, 5, 2, and 1 nodes. The final evaluation only tested 10 nodes cases. \
When run with 10 nodes, each node will parallel process one chromosome.

## Naming
- deception : the scripts are used in PNNL Deception cluster.
- pfs : the shared storage used is BeeGFS.
- nfs : the shared storage used is NFS.
- shm : the node-local storage is a tmpfs.
- ssd : the node-local storage is a SSD.
- staged : the shared data are staged-in to local nodes before workflow starts.

## Path Variables
- `SHARE` : the shared storage directory, either PFS or NFS
- `LOCAL` : the node-local storage directory, either the tmpfs at /dev/shm or the SSD at /scratch
- `SCRIPT_DIR` : Original script directory.
- `CURRENT_DIR` : Initial files and intermediate file directory.

## Others
The original test scripts are located on NFS, for PFS tests, all data in SCRIPT_DIR and CURRENT_DIR must be setup on the PFS path before tests start.