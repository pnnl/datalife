# Test Scripts used for paper final evaluations
These scripts are for the multiple configurations used in the evluations for 1000genome. \
They will produce performance numbers for Figure 6 in the paper.
There are three scripts: `1kgenome_15nodes_nfs_bfs.sbatch`, `1kgenome_10nodes_nfs_bfs.sbatch`, and `1kgenome_10nodes_shm_ssd_nfs_bfs_staging.sbatch`. \
- `1kgenome_15nodes_nfs_bfs.sbatch` is for distribution of 1000genome on 15 compute nodes. In that case each compute node will run 20 tasks at once. In the script, make sure you change the `CURRENT_DIR` following the instructions highlighted by !!!. The `CURRENT_DIR` specifies an NFS/BFS path where the current directory is located.  
- `1kgenome_10nodes_nfs_bfs.sbatch` is for distribution of 1000genome on 10 compute nodes, where each compute node will run 30 tasks in a pipeline. In the script, make sure you change the `CURRENT_DIR` following the instructions highlighed by !!!. The `CURRENT_DIR` is the PWD that is located in NFS/BFS for different setups.
- `1kgenome_10nodes_shm_ssd_nfs_bfs_staging.sbatch` is for distribution of 1000genome on 10 compute nodes, different from the above one, this script provides options to enable intermediate data generated and processed on RAMDISK/SSD and stage the input files to local RAMDISK/SSD. Please check !!! in the script for instructions for changes that you need to make. In particular, you have to pass three arguments to submission of the script for choosing NFS/BFS, SHM/SSD, and staging of the input or not. Also, make sure you change the `LOCAL_STORE` to a path under your own node-local RAMDISK/SSD. Furthermore, change the `CURRENT_DIR` to a path under NFS/BFS.  
- `1kgenome_bin` where you will find all 1000genome executables that are called in the sbatch scripts.

## Naming
- bfs : the shared storage used is BeeGFS.
- nfs : the shared storage used is NFS.
- shm : the node-local storage is a RAMDISK or tmpfs.
- ssd : the node-local storage is a SSD.
- staged : the input data are staged-in to local nodes before workflow starts.

