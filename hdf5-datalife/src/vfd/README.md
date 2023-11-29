# HDF5 Tracker VFD

## 1. Description
The HDF5 Tracker VFD is a [Virtual File
Driver](https://portal.hdfgroup.org/display/HDF5/Virtual+File+Drivers) (VFD) for
HDF5 that can be used to interface with the Tracker API. The driver is built as a
plugin library that is external to HDF5.

## 2. Dependencies
The Tracker VFD requires [HDF5](https://github.com/HDFGroup/hdf5) >= `1.14.0`,
which is the version that first introduced dynamically loadable VFDs.

## 3. Usage
To use the HDF5 Tracker VFD in an HDF5 application, the driver can either be
linked with the application during compilation, or it can be dynamically loaded
via an environment variable. It is more convenient to load the VFD as a dynamic
plugin because it does not require code changes or recompilation.

### Method 1: Dynamically loaded by environment variable (recommended)

As of HDF5 `1.14.0` each file in an HDF5 app opened or created with the default
file access property list (FAPL) will use the VFD specified in the `HDF5_DRIVER`
environment variable rather than the default "sec2" (POSIX) driver. To use the
Tracker VFD, simply set

```sh
HDF5_DRIVER=hdf5_tracker_vfd
```

Now we must tell the HDF5 library where to find the Tracker VFD. That is done
with the following environment variable:

```sh
HDF5_PLUGIN_PATH=<tracker_build_dir>/lib/vfd
```

The Tracker VFD has two configuration options.
1. persistence - if `true`, the HDF5 application will produce the same output
   files with the Tracker VFD as it would without it. If `false`, the files are
   buffered in Tracker during the application run, but are not persisted when the
   application terminates. Thus, no files are produced.
2. page size - The Tracker VFD works by mapping HDF5 files into its internal data
   structures. This happens in pages. The `page size` configuration option
   allows the user to specify the size, in bytes, of these pages. If your app
   does lots 2 MiB writes, then it's a good idea to set the page size to 2
   MiB. A smaller page size, 1 KiB for example, would convert each 2 MiB write
   into 2048 1 KiB writes. However, be aware that using pages that are too large
   can slow down metadata operations, which are usually less than 2 KiB. To
   combat the tradeoff between small metadata pages and large raw data pages,
   the Tracker VFD can be stacked underneath the [split VFD](https://docs.hdfgroup.org/hdf5/develop/group___f_a_p_l.html#ga502f1ad38f5143cf281df8282fef26ed).


These two configuration options are passed as a space-delimited string through
an environment variable:

```sh
# Example of configuring the Tracker VFD with `persistent_mode=true` and
# `page_size=64KiB`
HDF5_DRIVER_CONFIG="true 65536"
```

Finally we need to provide a configuration file for Tracker itself, and
`LD_PRELOAD` the Tracker VFD so we can intercept HDF5 calls.

Heres is a full example of running an HDF5 app with the Tracker VFD:

```sh
HDF5_DRIVER_CONFIG="true ${TRACKER_VFD_PAGE_SIZE}" \
   HDF5_DRIVER=hdf5_tracker_vfd \
   HDF5_LOG_FILE_PATH="${schema_file}" \
   HDF5_PLUGIN_PATH=$TRACKER_BLD_DIR/vfd \
   ./my_hdf5_app
```



## TODO
- For better memory utilization, may change I/O logging to be dependent on the selected page_size resolution, instead of per access.


<!-- ### Method 2: Linked into application

To link the Tracker VFD into an HDF5 application, the application should include
the `H5FDtracker.h` header and should link the installed VFD library,
`libhdf5_tracker_vfd.so`, into the application. Once this has been done, Tracker
VFD access can be set up by calling `H5Pset_fapl_tracker()` on a FAPL within the
HDF5 application. In this case, the `persistence` and `page_size` configuration
options are pass directly to this function:

```C
herr_t H5Pset_fapl_tracker(hid_t fapl_id, hbool_t persistence, size_t page_size)
```

The resulting `fapl_id` should then be passed to each file open or creation for
which you wish to use the Tracker VFD.

-->
