// #include <openssl/md5.h>
// #include <bsd/md5.h>

// #include "/home/mtang11/spack/opt/spack/linux-centos7-skylake_avx512/gcc-7.3.0/openssl-1.1.1q-kqr6gf43vvc4kxk3m5d3ozopr7fq5c4s/include/openssl/md5.h"
#include <stdlib.h>
#include <string.h>

#include "hdf5.h"
#include "tracker_vol.h"

// #include "../utils/md5/md5.h"
// #include <openssl/md5.h>
#include "../utils/uthash/src/uthash.h"


/************/
/* Typedefs */
/************/

typedef struct H5VL_tkr_dataset_info_t dataset_tkr_info_t;
typedef struct H5VL_tkr_group_info_t group_tkr_info_t;
typedef struct H5VL_tkr_datatype_info_t datatype_tkr_info_t;
typedef struct H5VL_tkr_attribute_info_t attribute_tkr_info_t;
typedef struct H5VL_tkr_file_info_t file_tkr_info_t;

typedef struct H5VL_tkr_blob_info_t blob_tkr_info_t;

unsigned long FILE_SORDER;
// unsigned long FILE_PORDER;

unsigned long DATA_SORDER;
// unsigned long DSET_PORDER;

unsigned long BLOB_SORDER;
unsigned long BLOB_PORDER;

// this HEADER_SIZE assumes all dataset 
// header size are the same at all time (no update)
// Only recorded at H5VLfile_create time
// hsize_t HEADER_SIZE; // candice added

/* VFD uses vars*/
unsigned long VFD_ACCESS_IDX;
unsigned long TOTAL_VFD_READ;
unsigned long TOTAL_VFD_WRITE;

unsigned long START_ADDR;
unsigned long END_ADDR;
unsigned long ACC_SIZE;
unsigned long START_PAGE;
unsigned long END_PAGE;

int TASK_ID = 0;



// unsigned long VFD_ADDR ;
// extern haddr_t vfd_start_addr;


/* candice added local prototypes */
// static int get_native_info(void *obj, H5I_type_t target_obj_type, hid_t connector_id,
//                        hid_t dxpl_id, H5O_info2_t *oinfo);

/* DsetTracker Related Code Start */
/* candice added for tracking object access start */

/* candice added for tracking object access end */

typedef struct TrackerHelper {
    /* Tracker properties */
    char* tkr_file_path;
    FILE* tkr_file_handle;
    Track_level tkr_level;
    char* tkr_line_format;
    char user_name[32];
    int pid;
    pthread_t tid;
    char proc_name[64];
    int ptr_cnt;
    int opened_files_cnt;
    file_tkr_info_t* opened_files;//linkedlist,
    
    /* candice added fields start */
    size_t tracker_page_size;
    // int vfd_opened_files_cnt;
    // vfd_file_tkr_info_t* vfd_opened_files;//linkedlist,
    /* candice added fields end*/
} tkr_helper_t;

typedef struct H5VL_tracker_t {
    hid_t  under_vol_id;        /* ID for underlying VOL connector */
    void  *under_object;        /* Info object for underlying VOL connector */
    H5I_type_t my_type;         /* obj type, dataset, datatype, etc. */
    tkr_helper_t *tkr_helper; /* pointer shared among all layers, one per process. */
    void *generic_tkr_info;    /* Pointer to a class-specific tracker info struct. */
                                /* Should be cast to layer-specific type before use, */
                                /* such as file_tkr_info, dataset_tkr_info. */
} H5VL_tracker_t;

// TODO(candice) : add for blob object
typedef struct H5VL_tracker_blob_t {
    int my_type;         /* obj type, dataset, datatype, etc. */
    // tkr_helper_t *tkr_helper; /* pointer shared among all layers, one per process. */
    blob_tkr_info_t * blob_info;    /* Pointer to blob tracker info struct. */
} H5VL_tracker_blob_t;

/* The TRACKER VOL wrapper context */
typedef struct H5VL_tracker_wrap_ctx_t {
    tkr_helper_t *tkr_helper; /* shared pointer */
    hid_t under_vol_id;         /* VOL ID for under VOL */
    void *under_wrap_ctx;       /* Object wrapping context for under VOL */
    file_tkr_info_t *file_info;
    unsigned long file_no;
    hid_t dtype_id;             /* only used by datatype */
} H5VL_tracker_wrap_ctx_t;

//======================================= statistics =======================================
//typedef struct H5VL_tkr_t {
//    void   *under_object;
//    char* func_name;
//    int func_cnt;//stats
//} H5VL_tkr_t;



struct H5VL_tkr_file_info_t {//assigned when a file is closed, serves to store stats (copied from shared_file_info)
    tkr_helper_t* tkr_helper;  //pointer shared among all layers, one per process.
    const char* file_name;
    unsigned long file_no;

    /* candice added for more stats start */
    char* intent; // TODO: convert to unsigned int type for less conversion
    char* task_name;
    unsigned long sorder_id;
    hid_t fapl_id;
    hsize_t file_size;
    hsize_t header_size;
    size_t  sieve_buf_size;
    hsize_t alignment;
    hsize_t threshold;
    unsigned long open_time;
    /* candice added for more stats end */

#ifdef H5_HAVE_PARALLEL
    // Only present for parallel HDF5 builds
    MPI_Comm mpi_comm;           // Copy of MPI communicator for file
    MPI_Info mpi_info;           // Copy of MPI info for file
    hbool_t mpi_comm_info_valid; // Indicate that MPI Comm & Info are valid
#endif /* H5_HAVE_PARALLEL */
    int ref_cnt;

    /* Currently open objects */
    int opened_datasets_cnt;
    dataset_tkr_info_t *opened_datasets;
    int opened_grps_cnt;
    group_tkr_info_t *opened_grps;
    int opened_dtypes_cnt;
    datatype_tkr_info_t *opened_dtypes;
    int opened_attrs_cnt;
    attribute_tkr_info_t *opened_attrs;

    /* Statistics */
    int ds_created;
    int ds_accessed;
    int grp_created;
    int grp_accessed;
    int dtypes_created;
    int dtypes_accessed;

    file_tkr_info_t *next;
};

// Common tracker information, for all objects
typedef struct H5VL_tkr_object_info_t {
    tkr_helper_t *tkr_helper;         //pointer shared among all layers, one per process.
    file_tkr_info_t *file_info;        // Pointer to file info for object's file
    H5O_token_t token;                  // Unique ID within file for object
    char *name;                         // Name of object within file
                                        // (possibly NULL and / or non-unique)
    int ref_cnt;                        // # of references to this tracker info
} object_tkr_info_t;

struct H5VL_tkr_dataset_info_t {
    object_tkr_info_t obj_info;        // Generic tracker. info
                                        // Must be first field in struct, for
                                        // generic upcasts to work
    

    H5T_class_t dt_class;               //data type class
    H5S_class_t ds_class;               //data space class
    char * layout;                      
    unsigned int dimension_cnt;
    // hsize_t dimensions[H5S_MAX_RANK];
    hsize_t * dimensions; // candice: save space
    size_t dset_type_size;
    // hsize_t dset_space_size;            //same as nelements!
    int dataset_read_cnt;
    int dataset_write_cnt;

    /* candice added for more dset stats start */
    // hid_t dset_id;                   // this should own by application
    char * pfile_name;                  // parent file name
    char *dset_name;
    unsigned long start_time;
    haddr_t dset_offset;
    hsize_t storage_size;
    size_t dset_n_elements;
    ssize_t hyper_nblocks;
    hid_t dspace_id;
    hid_t dtype_id;
    unsigned long sorder_id;
    unsigned long pfile_sorder_id;      // parent file sequential order ID
    size_t data_file_page_start;
    size_t data_file_page_end;
    size_t metadata_file_page;          // usually the token number

    // size_t metadata_file_page_end; // TODO: how to obtain the end?
    char * dset_select_type;
    size_t dset_select_npoints;

    /* candice added for more dset stats end */


    /* candice added for recording blob start */
    int blob_put_cnt;
    size_t total_bytes_blob_put;
    hsize_t total_blob_put_time;
    int blob_get_cnt;
    size_t total_bytes_blob_get;
    hsize_t total_blob_get_time;

    int used_blob_cnt;
    blob_tkr_info_t * used_blobs;
    /* candice added for recording blob end */
#ifdef H5_HAVE_PARALLEL
    int ind_dataset_read_cnt;
    int ind_dataset_write_cnt;
    int coll_dataset_read_cnt;
    int coll_dataset_write_cnt;
    int broken_coll_dataset_read_cnt;
    int broken_coll_dataset_write_cnt;
#endif /* H5_HAVE_PARALLEL */
    int access_cnt;

    dataset_tkr_info_t *next;
};



struct H5VL_tkr_group_info_t {
    object_tkr_info_t obj_info;        // Generic tracker. info
                                        // Must be first field in struct, for
                                        // generic upcasts to work

    int func_cnt;//stats
//    int group_get_cnt;
//    int group_specific_cnt;

    group_tkr_info_t *next;
};

typedef struct H5VL_tkr_link_info_t {
    int link_get_cnt;
    int link_specific_cnt;
} link_tkr_info_t;

struct H5VL_tkr_datatype_info_t {
    object_tkr_info_t obj_info;        // Generic tracker. info
                                        // Must be first field in struct, for
                                        // generic upcasts to work

    hid_t dtype_id;
    int datatype_commit_cnt;
    int datatype_get_cnt;

    datatype_tkr_info_t *next;
};

struct H5VL_tkr_attribute_info_t {
    object_tkr_info_t obj_info;        // Generic tracker. info
                                        // Must be first field in struct, for
                                        // generic upcasts to work

    int func_cnt;//stats
    unsigned long sorder_id;
    int attr_read_cnt;
    int attr_write_cnt;

    attribute_tkr_info_t *next;
};




// TODO(candice) : added blob type for tracking
struct H5VL_tkr_blob_info_t {
    object_tkr_info_t obj_info;        // Generic tracker. info
                                        // Must be first field in struct, for
                                        // generic upcasts to work
    // dataset_tkr_info_t
    
    /* candice added for recording blob start */
    // int blob_put_cnt;
    // size_t total_bytes_blob_put;
    // hsize_t total_blob_put_time;
    // int blob_get_cnt;
    // size_t total_bytes_blob_get;
    // hsize_t total_blob_get_time;
    /* candice added for recording blob end */
    int blob_id;
    int access_size;
    int sorder_id;
    int porder_id;
    int pdset_sorder_id;
    int pdset_porder_id;
    int pfile_sorder_id;
    int pfile_porder_id;

#ifdef H5_HAVE_PARALLEL
    int ind_blob_get_cnt;
    int ind_blob_put_cnt;
    int coll_blob_get_cnt;
    int coll_blob_put_cnt;
    int broken_coll_blob_get_cnt;
    int broken_coll_dblob_put_cnt;
#endif /* H5_HAVE_PARALLEL */
    int access_type; // 0 for put, 1 for get
    int access_cnt;
    
    blob_tkr_info_t *next;
};



// static int get_native_info(void *obj, H5I_type_t target_obj_type, hid_t connector_id,
//                        hid_t dxpl_id, H5O_info2_t *oinfo)
// {
//     H5VL_object_get_args_t vol_cb_args; /* Arguments to VOL callback */
//     H5VL_loc_params_t loc_params;

//     /* Set up location parameter */
//     _new_loc_pram(target_obj_type, &loc_params);

//     /* Set up VOL callback arguments */
//     vol_cb_args.op_type              = H5VL_OBJECT_GET_INFO;
//     vol_cb_args.args.get_info.oinfo  = oinfo;
//     vol_cb_args.args.get_info.fields = H5O_INFO_BASIC;

//     if(H5VLobject_get(obj, &loc_params, connector_id, &vol_cb_args, dxpl_id, NULL) < 0)
//         return -1;

//     return 0;
// }




/* Dataset Tracking Object Start */
typedef struct H5VL_dset_track_t dset_track_t;

typedef struct {
    char * key;            // Key for the hash table entry
    dset_track_t *dset_track_info;  // Value associated with the key
    bool logged;             // Whether the entry has been logged
    UT_hash_handle hh;              // Uthash handle
} DsetTrackHashEntry;

typedef struct myll_t {
    unsigned long data;
    struct myll_t* next;
} myll_t;

typedef struct H5VL_dset_track_t {
    // char *file_name;    // Parent file name
    // char *dset_name;     // Dataset name
    char * task_name;
    unsigned long start_time; // Start time of the dataset
    unsigned long end_time; // End time of the dataset
    size_t token_num;                   // Token number
    H5T_class_t dt_class;               // Data type class
    H5S_class_t ds_class;               // Data space class
    char *layout;
    unsigned int dimension_cnt;
    hsize_t *dimensions;
    size_t dset_type_size;
    int dataset_read_cnt;
    int dataset_write_cnt;
    size_t total_bytes_read;
    size_t total_bytes_written;
    haddr_t dset_offset;
    hsize_t storage_size;
    size_t dset_n_elements;
    ssize_t hyper_nblocks;


    unsigned long pfile_sorder_id;
    size_t data_file_page_start;
    size_t data_file_page_end;

    myll_t * sorder_ids; // linked-list head pointer
    myll_t * sorder_ids_end; // linked-list tail pointer
    myll_t * metadata_file_pages; // linked-list head pointer
    myll_t * metadata_file_pages_end; // linked-list tail pointer

    char * dset_select_type; // TODO: use to check for bytes
    size_t dset_select_npoints;
    // int access_cnt;
    // dset_track_t *next;
} dset_track_t;



/* Dataset Tracking Object End */



/* lock */
typedef struct {
    pthread_mutex_t mutex;
} TKRLock;

typedef struct {
    pthread_mutex_t mutex;
    DsetTrackHashEntry *hash_table;
} HASHLock;

// Global variable for the hash table
HASHLock lock;