# Note on Object Reuse on contact_map
Object Reuse on contact_map (in VOL, without VFD): 
* On the HDF5 VOL layer, found a [object token](https://forum.hdfgroup.org/t/hdf5-1-12-0-beta-release-is-available-for-testing/6637) type that seems is able to reveal pattern in subset data reuse
    * The object token type represent unique and permanent identifiers for referencing HDF5 objects within a container, designed to replace object address (where it may not be meaningful)
* A single operation must be called along with this token to reveal the unique blob_buffer address (e.g. a print statement must print token first then print the buffer)
    * Observe some pattern shown in SIM and AGG.
        * SIM: some object is write multiple times (VFD, there is no observation of address reuse)

        * AGG: unique object is read/write different times, but the read and write times are the same
            * There seems to be some access ordering in the read/write of a unique blob (On VFD, the file address are out-of-order, but does not show reuse)

## Unique object access example
```
size_of_token : 16
{ H5VLblob_put(1) : {blob_id : 0x300236c, access_size : 9204, }}
{ track_blob(1) : {token : 0x7ffdace9ef40, blob_id : 0x7f9902e1b80b, access_size : 50340716, buffer : 0x23f4 }}
{ track_blob(2) : {blob_id : 0x300236c, access_size : 9204, buffer : 0x2ff9250, token : 0x7ffdace9ef40 }}
{ H5VLblob_put(2) : {blob_id : 0x300236c, access_size : 9204, }}
```

## Object reuse example on aggregate.py
In log line 80+, read phase:
```
PROVENANCE VOL BLOB Get
{ H5VLblob_put : {blob_id : 0x292e90c, access_size : 9212, }}
{ track_blob : {buffer : 0x23fc }}
```
In log line 998 +, write phase:
```
PROVENANCE VOL BLOB Put
{ H5VLblob_put : {blob_id : 0x292e8fc, access_size : 9212, }}
{ track_blob : {buffer : 0x23fc }}
```




