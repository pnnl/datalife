# Note for I/O related object operations
Should only be recording objects that affects I/O


## Observation with WRF + PyFlextrkr
In summary, only file and datasets are relavent to I/O. Others should be recorded as metadata
- attributes op does not seem to incur I/O
- blob_put/get are only in memory, except for `H5T_VLEN`
- group operation may incur I/O (metadata)
- dataset metadata I/O are significant, observed from NetCDF4 files
- `object_get` may incur I/O when data is not already in memory


# TODO:
## small fixes:
- if taskname is not provided, get from pid
- output different pid for each task, not the initially saved pid

## Observation with DDMD
- TODO: need to test with actual DDMD program
    - Hermes 1.0 has problem running DDMD
### bservation with co-scheduling
- blob_put/get with `H5T_VLEN` that are large in size, may get particular blob file offset with structured print