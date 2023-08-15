# PyFLEXTRKR Workflow Data Access Example
The wrofklow schema is collected with single process sequentil run on 10 stages workflow.

## VFD Graph
The VFD yaml stat is output from a HDF5 Virtual File Driver (VFD) plugin

The edges flows from left to right:
- Files are placed with it's first access time.
- Flow from file to tasks, edge width is graphed with actual I/O access size
- Flow from task to file, edge is graphed with file size
- Edge width is graphed with square-root to reduce variability, but the number represents actual access size and actual bandwidth.

The task_to_file stat is extracted from the VFD stat to simply represent task to file dependencies

## VOL Graph
The VOL yaml stats is output from HDF5 Virtual Object Layter (VOL) plugin.


Currently only shows 2 stages due to large too large.


The edges flows from left to right:
- "read_write" access is currently represented with flow from left to right as well
- Dataset is placed as it's last access time.
- File is placed as it's first access time.
- Flow from file-to-dataset and dataset-to-task is graphed with dataset access size (memory access volume)
- Flow from dataset-to-file is graphed with actual file size
- Edge width is graphed with square-root to reduce variability, but the number represents actual access size and actual bandwidth.