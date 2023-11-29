# Note

## TODO: Modification on VFD output
3. In VFD, add entry for `op_count_range` to record the largest I/O operation index (for prefetcher use). (?)


## Done: Modification on VFD output
1. need to modify "task" entry format for VFD (done)
```
- Task:
  task_pid: xxx
  VFD-Total-Overhead(ms): 0
```
2. In VFD traces, identify file access type (done)
- read-only and write-only files label them as "input" and "output". (done)
- Others label "read-write" for now, later can identify mode detail pattern. (done)
4. Check if VFD "data" is correctly format (done)
5. Make all `- H5FD_MEM_XXX:` not a list item, they all listed as dictionary (done)
6. Make ranges values with list not tuple (done)
7. Record `HERMES_PAGE_SIZE` in VFD log! (done)


## TODO: Modification on VOL output
1. need to modify "task" entry format for VOL (done)
```
- Task:
  task_pid: xxx
  VOL-Total-Overhead(ms): 0
```


## stat_to_prefetcher
1. Add a file count to know the number of input/output files (for reuse file label in graph) 
2. Keep complete filepath for generating prefetch schema in Hermes!