#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h> // Include for file locks
#include <errno.h>

#include "hdf5.h"
#include "tracker_vol.h"
#include "tracker_vol_types.h"

/**********/
/* Macros */
/**********/

/* Whether to display log messge when callback is invoked */
/* (Uncomment to enable) */
/* #define TRACKER_PT_LOGGING */
/* Hack for missing va_copy() in old Visual Studio editions
 * (from H5win2_defs.h - used on VS2012 and earlier)
 */
#if defined(_WIN32) && defined(_MSC_VER) && (_MSC_VER < 1800)
#define va_copy(D,S)      ((D) = (S))
#endif

#define STAT_FUNC_MOD 733//a reasonably big size to avoid expensive collision handling, make sure it works with 62 function names.

#define MAX_PATH_LENGTH 1024

// #define UINT32DECODE(p, val) {                      \
//     (val)  = (uint32_t)(*p++) << 24;                \
//     (val) |= (uint32_t)(*p++) << 16;                \
//     (val) |= (uint32_t)(*p++) <<  8;                \
//     (val) |= (uint32_t)(*p++);                      \
// }



/************/
/* Typedefs */
/************/
static tkr_helper_t* TKR_HELPER = NULL;
unsigned long TOTAL_TKR_OVERHEAD;
unsigned long TOTAL_NATIVE_H5_TIME;
unsigned long TKR_WRITE_TOTAL_TIME;
unsigned long FILE_LL_TOTAL_TIME;       //record file linked list overhead
unsigned long DS_LL_TOTAL_TIME;         //dataset
unsigned long GRP_LL_TOTAL_TIME;        //group
unsigned long DT_LL_TOTAL_TIME;         //datatype
unsigned long ATTR_LL_TOTAL_TIME;       //attribute
//shorten function id: use hash value
static char* FUNC_DIC[STAT_FUNC_MOD];


/* locks */
void tkrLockInit(TKRLock* lock) {
    pthread_mutex_init(&lock->mutex, NULL);
}

void tkrLockAcquire(TKRLock* lock) {
    pthread_mutex_lock(&lock->mutex);
}

void tkrLockRelease(TKRLock* lock) {
    pthread_mutex_unlock(&lock->mutex);
}

void tkrLockDestroy(TKRLock* lock) {
    pthread_mutex_destroy(&lock->mutex);
}

TKRLock myLock;


/* Common Routines */
static
unsigned long get_time_usec(void) {
    struct timeval tp;

    gettimeofday(&tp, NULL);
    return (unsigned long)((1000000 * tp.tv_sec) + tp.tv_usec);
}


//======================================= statistics =======================================

/********************* */
/* Function prototypes */
/********************* */
/* Helper routines  */
int getCurrentTask(char **curr_task);
static H5VL_tracker_t *H5VL_tracker_new_obj(void *under_obj,
    hid_t under_vol_id, tkr_helper_t* helper);
static herr_t H5VL_tracker_free_obj(H5VL_tracker_t *obj);

/* Tracker Callbacks prototypes */
static hid_t dataset_get_type(void *under_dset, hid_t under_vol_id, hid_t dxpl_id);
static hid_t dataset_get_space(void *under_dset, hid_t under_vol_id, hid_t dxpl_id);
static hid_t dataset_get_dcpl(void *under_dset, hid_t under_vol_id, hid_t dxpl_id);
static ssize_t attr_get_name(void *under_obj, hid_t under_vol_id, hid_t dxpl_id,
    size_t buf_size, void *buf);

     /* candice added routine prototypes start */
char * file_get_intent(void *under_file, hid_t under_vol_id, hid_t dxpl_id);
static hsize_t file_get_size(void *under_file, hid_t under_vol_id, hid_t dxpl_id);
char * dataset_get_layout(hid_t plist_id);
static hsize_t dataset_get_storage_size(void *under_dset, hid_t under_vol_id, hid_t dxpl_id);
static haddr_t dataset_get_offset(void *under_dset, hid_t under_vol_id, hid_t dxpl_id);
static hsize_t dataset_get_num_chunks(void *under_dset, hid_t under_vol_id, hid_t dxpl_id);
static hsize_t dataset_get_vlen_buf_size(void *under_dset, hid_t under_vol_id, hid_t dxpl_id); //TODO: fix
static herr_t dataset_get_chunk_info_by_coord(void *under_dset, hid_t under_vol_id, hid_t dxpl_id,
    const hsize_t *offset, unsigned *filter_mask /*out*/, haddr_t *addr /*out*/, hsize_t *size /*out*/); //TODO: fix
static herr_t dataset_get_chunk_info_by_idx(void *under_dset, hid_t under_vol_id, hid_t dxpl_id,
    hid_t fspace_id, hsize_t chk_index,
    hsize_t *offset /*out*/, unsigned *filter_mask /*out*/, haddr_t *addr /*out*/, hsize_t *size /*out*/);
     /* candice added routine prototypes end */

/* Local routine prototypes */
tkr_helper_t* tkr_helper_init( char* file_path, Track_level tkr_level, char* tkr_line_format);
datatype_tkr_info_t *new_dtype_info(file_tkr_info_t* root_file,
    const char *name, H5O_token_t token);
dataset_tkr_info_t *new_dataset_info(file_tkr_info_t *root_file,
    const char *name, H5O_token_t token);
group_tkr_info_t *new_group_info(file_tkr_info_t *root_file,
    const char *name, H5O_token_t token);
attribute_tkr_info_t *new_attribute_info(file_tkr_info_t *root_file,
    const char *name, H5O_token_t token);
file_tkr_info_t *new_file_info(const char* fname, unsigned long file_no);
void dtype_info_free(datatype_tkr_info_t* info);
void file_info_free(file_tkr_info_t* info);
void group_info_free(group_tkr_info_t* info);
void dataset_info_free(dataset_tkr_info_t* info);
void attribute_info_free(attribute_tkr_info_t *info);
datatype_tkr_info_t * add_dtype_node(file_tkr_info_t *file_info,
    H5VL_tracker_t *dtype, const char *obj_name, H5O_token_t token);
int rm_dtype_node(tkr_helper_t *helper, void *under, hid_t under_vol_id, datatype_tkr_info_t *dtype_info);
group_tkr_info_t * add_grp_node(file_tkr_info_t *root_file,
    H5VL_tracker_t *upper_o, const char *obj_name, H5O_token_t token);
int rm_grp_node(tkr_helper_t *helper, void *under_obj, hid_t under_vol_id, group_tkr_info_t *grp_info);
attribute_tkr_info_t * add_attr_node(file_tkr_info_t *root_file,
    H5VL_tracker_t *attr, const char *obj_name, H5O_token_t token);
int rm_attr_node(tkr_helper_t *helper, void *under_obj, hid_t under_vol_id, attribute_tkr_info_t *attr_info);
file_tkr_info_t * add_file_node(tkr_helper_t* helper, const char* file_name, unsigned long file_no);
int rm_file_node(tkr_helper_t* helper, unsigned long file_no);
file_tkr_info_t * _search_home_file(unsigned long obj_file_no);
dataset_tkr_info_t * add_dataset_node(unsigned long obj_file_no, H5VL_tracker_t *dset, H5O_token_t token,
        file_tkr_info_t* file_info_in, const char* ds_name, hid_t dxpl_id, void** req);
int rm_dataset_node(tkr_helper_t *helper, void *under_obj, hid_t under_vol_id, dataset_tkr_info_t *dset_info);
void ptr_cnt_increment(tkr_helper_t* helper);
void ptr_cnt_decrement(tkr_helper_t* helper);
void get_time_str(char *str_out);
dataset_tkr_info_t * new_ds_tkr_info(void* under_object, hid_t vol_id, H5O_token_t token,
        file_tkr_info_t* file_info, const char* ds_name, hid_t dxpl_id, void **req);
void _new_loc_pram(H5I_type_t type, H5VL_loc_params_t *lparam);

static int get_native_info(void *obj, H5I_type_t target_obj_type, hid_t connector_id,
                       hid_t dxpl_id, H5O_info2_t *oinfo);
static int get_native_file_no(const H5VL_tracker_t *file_obj, unsigned long *file_num_out);
H5VL_tracker_t * _file_open_common(void* under, hid_t vol_id, const char* name);


herr_t tracker_file_setup(const char* str_in, char* file_path_out, Track_level* level_out, char* format_out);
H5VL_tracker_t * _fake_obj_new(file_tkr_info_t* root_file, hid_t under_vol_id);
void _fake_obj_free(H5VL_tracker_t* obj);
H5VL_tracker_t * _obj_wrap_under(void* under, H5VL_tracker_t* upper_o,
        const char *name, H5I_type_t type, hid_t dxpl_id, void** req);

unsigned int genHash(const char *msg);
void _dic_init(void);
void _dic_free(void);


/* Tracker internal print and logs prototypes */
void _dic_print(void);
void _preset_dic_print(void);
void file_stats_tkr_write(const file_tkr_info_t* file_info);
void dataset_stats_tkr_write(const dataset_tkr_info_t* ds_info);
void datatype_stats_tkr_write(const datatype_tkr_info_t* dt_info);
void group_stats_tkr_write(const group_tkr_info_t* grp_info);
void attribute_stats_tkr_write(const attribute_tkr_info_t *attr_info);
void tkr_helper_teardown(tkr_helper_t* helper);
int tkr_write(tkr_helper_t* helper_in, const char* msg, unsigned long duration);
char * get_datatype_class_str(hid_t type_id);


    
    /* candice added routine prototypes start */
// void log_file_stat_yaml(FILE *f, const file_tkr_info_t* file_info);
void log_file_stat_yaml(tkr_helper_t* helper_in, const file_tkr_info_t* file_info);
void log_dset_ht_yaml(FILE *f);
// void print_order_id();
// void tracker_insert_file(file_list_t** head_ref, file_tkr_info_t * file_info);
// void print_all_tracker(file_list_t * head);
// void tracker_file_dump(file_list_t * file_ndoe);
// void tracker_dset_dump(dset_list_t * dset_node);
// file_list_t * tracker_newfile(file_tkr_info_t * file_info);
void H5VL_arrow_get_selected_sub_region(hid_t space_id, size_t org_type_size);
void file_info_print(char * func_name, void * obj, hid_t fapl_id, hid_t fcpl_id, hid_t dxpl_id);
void dataset_info_print(char * func_name, hid_t mem_type_id, hid_t mem_space_id,
    hid_t file_space_id, void * obj, hid_t dxpl_id);
void blob_info_print(char * func_name, void * obj, hid_t dxpl_id, 
    size_t size, void * blob_id, const void * buf, void *ctx);


int is_page_in_list(size_t *list, size_t list_size, size_t page);
void print_list(const size_t *list, size_t list_size);
void add_meta_page_to_list(size_t **list, size_t *list_size, size_t page);


void init_hasn_lock();
void print_ht_token_numbers();
void free_dset_track_info(dset_track_t *dset_track_info);

int myll_print(myll_t *head);
void myll_to_file(FILE * file, myll_t *head);
void myll_free(myll_t **head);
void myll_add(myll_t **head, myll_t **tail, unsigned long new_data);

dset_track_t *create_dset_track_info(dataset_tkr_info_t* dset_info);
void remove_dset_track_info(char * key);
void update_dset_track_info(char * key, dataset_tkr_info_t* dset_info);
void add_dset_track_info(char * key, dset_track_t *dset_track_info);
void add_to_dset_ht(dataset_tkr_info_t* dset_info);

    /* candice added routine prototypes end */


/* Tracker objects prototypes */
void file_ds_created(file_tkr_info_t* info);
void file_ds_accessed(file_tkr_info_t* info);
void file_grp_created(file_tkr_info_t* info);
void file_grp_accessed(file_tkr_info_t* info);
void file_dtypes_created(file_tkr_info_t* info);
void file_dtypes_accessed(file_tkr_info_t* info);










/* Helper routines implementation */

int getCurrentTask(char **curr_task) {
    // Get environment variables
    const char *path_for_task_files = getenv("PATH_FOR_TASK_FILES");
    const char *workflow_name = getenv("WORKFLOW_NAME");

    if (path_for_task_files && workflow_name) {
        // Construct the full file path
        char file_path[MAX_PATH_LENGTH];
        snprintf(file_path, MAX_PATH_LENGTH, "%s/%s_vfd.curr_task", path_for_task_files, workflow_name);

        // Open the file with exclusive lock
        int fd = open(file_path, O_RDONLY);
        if (fd != -1) {
            struct flock lock;
            lock.l_type = F_RDLCK;
            lock.l_start = 0;
            lock.l_whence = SEEK_SET;
            lock.l_len = 0;

            // Attempt to obtain a shared lock on the file
            if (fcntl(fd, F_SETLK, &lock) != -1) {
                // Read the file content into a string
                FILE *file_stream = fopen(file_path, "r");
                if (file_stream) {
                    fseek(file_stream, 0, SEEK_END);
                    long file_size = ftell(file_stream);
                    fseek(file_stream, 0, SEEK_SET);

                    *curr_task = (char *)malloc(file_size + 1);
                    if (*curr_task) {
                        fread(*curr_task, 1, file_size, file_stream);
                        (*curr_task)[file_size] = '\0';
                        fclose(file_stream);
                        close(fd);  // Release the lock
                        return 1;
                    } else {
                        perror("Failed to allocate memory");
                    }
                } else {
                    fprintf(stderr, "Failed to read file: %s\n", file_path);
                }
            } else {
                fprintf(stderr, "Failed to obtain lock on file: %s\n", file_path);
            }
            close(fd);
        } else {
            fprintf(stderr, "Failed to open file: %s - %s\n", file_path, strerror(errno));
        }
    } else {
        fprintf(stderr, "Missing environment variables: PATH_FOR_TASK_FILES and/or WORKFLOW_NAME\n");
    }

    return 0;
}

size_t get_tracker_page_size() {
    const char* env_value = getenv("TRACKER_VFD_PAGE_SIZE");
    if (env_value != NULL) {
        // Convert the environment variable value to size_t
        size_t page_size = (size_t)strtoul(env_value, NULL, 10);
        return page_size;
    }
    // Return a default value if the environment variable is not set
    return DEFAULT_PAGE_SIZE; // Default page size of 8192 bytes
}

size_t token_to_num(H5O_token_t token){
    size_t token_number;
    char token_buffer[20]; // Assuming a maximum of 20 characters for the string representation
    sprintf(token_buffer, "%d", token); // Convert to string
    token_number = strtol(token_buffer, NULL, 10); // Convert string to long
    return token_number;
}



// Function to check if a page is already in the list
int is_page_in_list(size_t *list, size_t list_size, size_t page) {
    for (size_t i = 0; i < list_size; i++) {
        printf("Checking page %zu in the list\n", list[i]);
        if (list[i] == page) {
            return 1; // Page found in the list
        }
    }
    return 0; // Page not found in the list
}

void print_list(const size_t *list, size_t list_size) {
    printf("List contents: ");
    for (size_t i = 0; i < list_size; i++) {
        printf("%zu ", list[i]);
    }
    printf("\n");
}

void add_meta_page_to_list(size_t **list, size_t *list_size, size_t page) {
    // printf("Adding page %zu to the list\n", page);

    // Check if the page is already in the list
    if (!is_page_in_list(*list, *list_size, page)) {
        // Allocate memory for a larger list
        size_t new_list_size = *list_size + 1;
        size_t *new_list = realloc(*list, new_list_size * sizeof(size_t));
        if (new_list == NULL) {
            perror("Failed to allocate memory for the list");
            return;
        }
        // Update the list and size
        *list = new_list;
        (*list)[*list_size] = page;
        *list_size = new_list_size;
    }


}




/*-------------------------------------------------------------------------
 * Function:    H5VL__tracker_new_obj
 *
 * Purpose:     Create a new TRACKER object for an underlying object
 *
 * Return:      Success:    Pointer to the new TRACKER object
 *              Failure:    NULL
 *
 * Programmer:  Quincey Koziol
 *              Monday, December 3, 2018
 *
 *-------------------------------------------------------------------------
 */
static H5VL_tracker_t *
H5VL_tracker_new_obj(void *under_obj, hid_t under_vol_id, tkr_helper_t* helper)
{
//    unsigned long start = get_time_usec();
    H5VL_tracker_t *new_obj;

    assert(under_vol_id);
    assert(helper);

    new_obj = (H5VL_tracker_t *)calloc(1, sizeof(H5VL_tracker_t));
    new_obj->under_object = under_obj;
    new_obj->under_vol_id = under_vol_id;
    new_obj->tkr_helper = helper;
    ptr_cnt_increment(new_obj->tkr_helper);
    H5Iinc_ref(new_obj->under_vol_id);
    //TOTAL_TKR_OVERHEAD += (get_time_usec() - start);
    return new_obj;
} /* end H5VL__tracker_new_obj() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__tracker_free_obj
 *
 * Purpose:     Release a TRACKER object
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 * Programmer:  Quincey Koziol
 *              Monday, December 3, 2018
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL_tracker_free_obj(H5VL_tracker_t *obj)
{
    //unsigned long start = get_time_usec();
    hid_t err_id;

    assert(obj);

    ptr_cnt_decrement(TKR_HELPER);

    err_id = H5Eget_current_stack();

    H5Idec_ref(obj->under_vol_id);

    H5Eset_current_stack(err_id);

    free(obj);
    //TOTAL_TKR_OVERHEAD += (get_time_usec() - start);
    return 0;
} /* end H5VL__tracker_free_obj() */













/* Tracker Callbacks implementation */
herr_t object_get_name(void * obj, 
    const H5VL_loc_params_t * loc_params,
    hid_t connector_id,
    hid_t dxpl_id,
    void ** req)
{
    H5VL_object_get_args_t vol_cb_args; /* Arguments to VOL callback */

    /* Set up VOL callback arguments */
    vol_cb_args.op_type              = H5VL_OBJECT_GET_NAME;
    // vol_cb_args.args.get_type.type_id = H5I_INVALID_HID;

    if(H5VLobject_get(obj, loc_params, connector_id, &vol_cb_args, dxpl_id, req) < 0)
        return H5I_INVALID_HID;
}

static hid_t dataset_get_type(void *under_dset, hid_t under_vol_id, hid_t dxpl_id)
{
    H5VL_dataset_get_args_t vol_cb_args; /* Arguments to VOL callback */

    /* Set up VOL callback arguments */
    vol_cb_args.op_type              = H5VL_DATASET_GET_TYPE;
    vol_cb_args.args.get_type.type_id = H5I_INVALID_HID;

    if(H5VLdataset_get(under_dset, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0)
        return H5I_INVALID_HID;

    return vol_cb_args.args.get_type.type_id;
}

static hid_t dataset_get_space(void *under_dset, hid_t under_vol_id, hid_t dxpl_id)
{
    H5VL_dataset_get_args_t vol_cb_args; /* Arguments to VOL callback */

    /* Set up VOL callback arguments */
    vol_cb_args.op_type              = H5VL_DATASET_GET_SPACE;
    vol_cb_args.args.get_space.space_id = H5I_INVALID_HID;

    if(H5VLdataset_get(under_dset, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0)
        return H5I_INVALID_HID;

    return vol_cb_args.args.get_space.space_id;
}

static hid_t dataset_get_dcpl(void *under_dset, hid_t under_vol_id, hid_t dxpl_id)
{
    H5VL_dataset_get_args_t vol_cb_args; /* Arguments to VOL callback */

    /* Set up VOL callback arguments */
    vol_cb_args.op_type              = H5VL_DATASET_GET_DCPL;
    vol_cb_args.args.get_dcpl.dcpl_id = H5I_INVALID_HID;

    if(H5VLdataset_get(under_dset, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0)
        return H5I_INVALID_HID;

    return vol_cb_args.args.get_dcpl.dcpl_id;
}

static ssize_t attr_get_name(void *under_obj, hid_t under_vol_id, hid_t dxpl_id,
    size_t buf_size, void *buf)
{
    H5VL_attr_get_args_t vol_cb_args;        /* Arguments to VOL callback */
    size_t attr_name_len = 0;  /* Length of attribute name */

    /* Set up VOL callback arguments */
    vol_cb_args.op_type                           = H5VL_ATTR_GET_NAME;
    vol_cb_args.args.get_name.loc_params.type     = H5VL_OBJECT_BY_SELF;
    vol_cb_args.args.get_name.loc_params.obj_type = H5I_ATTR;
    vol_cb_args.args.get_name.buf_size            = buf_size;
    vol_cb_args.args.get_name.buf                 = (char*)buf;
    vol_cb_args.args.get_name.attr_name_len       = &attr_name_len;

    if (H5VLattr_get(under_obj, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0)
        return -1;

    return (ssize_t)attr_name_len;
}

char * file_get_intent(void *under_file, hid_t under_vol_id, hid_t dxpl_id)
{
    H5VL_file_get_args_t             vol_cb_args;               /* Arguments to VOL callback */
    unsigned                         intent_flags; /* Dataset's offset */

    /* Set up VOL callback arguments */
    vol_cb_args.op_type               = H5VL_FILE_GET_INTENT;
    vol_cb_args.args.get_intent.flags = &intent_flags;

    /* Get the size */
    H5VLfile_get(under_file, under_vol_id, &vol_cb_args, dxpl_id, NULL);

    if(intent_flags == H5F_ACC_RDWR)
        return "H5F_ACC_RDWR";
    else if(intent_flags == H5F_ACC_RDONLY)
        return "H5F_ACC_RDONLY";   
    else if(intent_flags == H5F_ACC_RDONLY)
        return "H5F_ACC_RDONLY";
    else if(intent_flags == H5F_ACC_SWMR_WRITE)
        return "H5F_ACC_SWMR_WRITE";
    else if(intent_flags == H5F_ACC_SWMR_READ)
        return "H5F_ACC_SWMR_READ";
    else
        return NULL;
}

// TODO: H5VL_FILE_GET_OBJ_COUNT with H5VLfile_get()
// TODO: H5VL_FILE_GET_OBJ_IDS with H5VLfile_get()

static hsize_t file_get_size(void *under_file, hid_t under_vol_id, hid_t dxpl_id)
{
    H5VL_optional_args_t                vol_cb_args;               /* Arguments to VOL callback */
    H5VL_native_file_optional_args_t file_opt_args;       /* Arguments for optional operation */
    hsize_t                             size; /* Dataset's offset */

    /* Set up VOL callback arguments */
    file_opt_args.get_size.size = &size;
    vol_cb_args.op_type         = H5VL_NATIVE_FILE_GET_SIZE;
    vol_cb_args.args            = &file_opt_args;

    /* Get the size */
    if (H5VLfile_optional(under_file, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0)
        return -1;
    
    return size;
}

char * dataset_get_layout(hid_t plist_id)
{
    H5D_layout_t layout = H5Pget_layout(plist_id);

    if(layout == H5D_COMPACT)
        return "H5D_COMPACT";
    else if(layout == H5D_CONTIGUOUS)
        return "H5D_CONTIGUOUS";
    else if(layout == H5D_CHUNKED)
        return "H5D_CHUNKED";
    else if(layout == H5D_VIRTUAL)
        return "H5D_VIRTUAL";
    else if(layout == H5D_NLAYOUTS)
        return "H5D_NLAYOUTS";
    else 
        return "H5D_LAYOUT_ERROR";
}

static hsize_t attribute_get_storage_size(void *under_attr, hid_t under_vol_id, hid_t dxpl_id)
{
    H5VL_attr_get_args_t vol_cb_args; /* Arguments to VOL callback */
    hsize_t                 storage_size = 0;

    /* Set up VOL callback arguments */
    vol_cb_args.op_type                            = H5VL_ATTR_GET_STORAGE_SIZE;
    vol_cb_args.args.get_storage_size.data_size    = &storage_size;

    if(H5VLattr_get(under_attr, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0)
        return H5I_INVALID_HID;

    return storage_size;
}

static hsize_t dataset_get_storage_size(void *under_dset, hid_t under_vol_id, hid_t dxpl_id)
{
    H5VL_dataset_get_args_t vol_cb_args; /* Arguments to VOL callback */
    hsize_t                 storage_size = 0;

    /* Set up VOL callback arguments */
    vol_cb_args.op_type                            = H5VL_DATASET_GET_STORAGE_SIZE;
    vol_cb_args.args.get_storage_size.storage_size = &storage_size;

    if(H5VLdataset_get(under_dset, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0)
        return H5I_INVALID_HID;

    return storage_size;
}

static haddr_t dataset_get_offset(void *under_dset, hid_t under_vol_id, hid_t dxpl_id)
{
    H5VL_optional_args_t                vol_cb_args;               /* Arguments to VOL callback */
    H5VL_native_dataset_optional_args_t dset_opt_args;             /* Arguments for optional operation */
    haddr_t                             dset_offset; /* Dataset's offset */

    /* Set up VOL callback arguments */
    dset_opt_args.get_offset.offset = &dset_offset;
    vol_cb_args.op_type             = H5VL_NATIVE_DATASET_GET_OFFSET;
    vol_cb_args.args                = &dset_opt_args;

    /* Get the offset */
    if (H5VLdataset_optional(under_dset, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0)
        return HADDR_UNDEF;
    
    return dset_offset;
}

static herr_t dataset_get_chunk_info_by_coord(void *under_dset, hid_t under_vol_id, hid_t dxpl_id,
    const hsize_t *offset, unsigned *filter_mask /*out*/, haddr_t *addr /*out*/, hsize_t *size /*out*/)
{
    H5VL_optional_args_t                vol_cb_args;    /* Arguments to VOL callback */
    H5VL_native_dataset_optional_args_t dset_opt_args;  /* Arguments for optional operation */
    herr_t                              ret_value = SUCCEED;

    /* Set up VOL callback arguments */
    dset_opt_args.get_chunk_info_by_coord.offset      = offset;
    dset_opt_args.get_chunk_info_by_coord.filter_mask = filter_mask;
    dset_opt_args.get_chunk_info_by_coord.addr        = addr;
    dset_opt_args.get_chunk_info_by_coord.size        = size;
    vol_cb_args.op_type                               = H5VL_NATIVE_DATASET_GET_CHUNK_INFO_BY_COORD;
    vol_cb_args.args                                  = &dset_opt_args;

    /* Call private function to get the chunk info given the chunk's index */
    if (H5VLdataset_optional(under_dset, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0){
        printf("H5VLdataset_optional-failed: \"can't get chunk info by its logical coordinates\", ");
        return FAIL;
    }

    return ret_value;
}

static herr_t dataset_get_chunk_info_by_idx(void *under_dset, hid_t under_vol_id, hid_t dxpl_id,
    hid_t fspace_id, hsize_t chk_index,
    hsize_t *offset /*out*/, unsigned *filter_mask /*out*/, haddr_t *addr /*out*/, hsize_t *size /*out*/)
{
    H5VL_optional_args_t                vol_cb_args;    /* Arguments to VOL callback */
    H5VL_native_dataset_optional_args_t dset_opt_args;  /* Arguments for optional operation */
    hsize_t                             nchunks   = 0;  /* Number of chunks */
    herr_t                              ret_value = SUCCEED;

    /* Set up VOL callback arguments */
    dset_opt_args.get_num_chunks.space_id = fspace_id;
    dset_opt_args.get_num_chunks.nchunks  = &nchunks;
    vol_cb_args.op_type                   = H5VL_NATIVE_DATASET_GET_NUM_CHUNKS;
    vol_cb_args.args                      = &dset_opt_args;

    /* Get the number of written chunks to check range */
    if (H5VLdataset_optional(under_dset, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0){
        printf("H5VLdataset_optional-failed: \"can't get number of chunks\", ");
        return FAIL;
    }

    /* Check range for chunk index */
    if (chk_index >= nchunks){
        printf("H5VLdataset_optional-failed: \"chunk index is out of range\", ");
        return FAIL;
    }

    /* Set up VOL callback arguments */
    // dset_opt_args.get_chunk_info_by_idx.space_id    = fspace_id;
    // dset_opt_args.get_chunk_info_by_idx.chk_index   = chk_index;
    dset_opt_args.get_chunk_info_by_idx.offset      = offset;
    dset_opt_args.get_chunk_info_by_idx.filter_mask = filter_mask;
    dset_opt_args.get_chunk_info_by_idx.addr        = addr;
    dset_opt_args.get_chunk_info_by_idx.size        = size;
    vol_cb_args.op_type                             = H5VL_NATIVE_DATASET_GET_CHUNK_INFO_BY_IDX;
    vol_cb_args.args                                = &dset_opt_args;

    /* Call private function to get the chunk info given the chunk's index */
    if (H5VLdataset_optional(under_dset, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0)
        return FAIL;
    
    return ret_value;
}

static hsize_t dataset_get_num_chunks(void *under_dset, hid_t under_vol_id, hid_t dxpl_id)
{
    H5VL_optional_args_t                vol_cb_args;               /* Arguments to VOL callback */
    H5VL_native_dataset_optional_args_t dset_opt_args;             /* Arguments for optional operation */
    hsize_t                             dset_num_chunks; /* Dataset's offset */

    /* Set up VOL callback arguments */
    dset_opt_args.get_num_chunks.nchunks = &dset_num_chunks;
    vol_cb_args.op_type             = H5VL_NATIVE_DATASET_GET_NUM_CHUNKS;
    vol_cb_args.args                = &dset_opt_args;

    /* Get the num chunks */
    if (H5VLdataset_optional(under_dset, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0)
        return -1;
    
    return dset_num_chunks;
}

static hsize_t dataset_get_vlen_buf_size(void *under_dset, hid_t under_vol_id, hid_t dxpl_id)
{
    H5VL_optional_args_t                vol_cb_args;               /* Arguments to VOL callback */
    H5VL_native_dataset_optional_args_t dset_opt_args;             /* Arguments for optional operation */
    hsize_t                             size; /* Dataset's offset */

    /* Set up VOL callback arguments */
    dset_opt_args.get_vlen_buf_size.size = &size;
    vol_cb_args.op_type             = H5VL_NATIVE_DATASET_GET_VLEN_BUF_SIZE;
    vol_cb_args.args                = &dset_opt_args;

    /* Get the vlen buf size */
    if (H5VLdataset_optional(under_dset, under_vol_id, &vol_cb_args, dxpl_id, NULL) < 0)
        return -1;
    
    return size;
}














/* Local routine implementation */

tkr_helper_t * tkr_helper_init( char* file_path, Track_level tkr_level, char* tkr_line_format)
{
   
    
    tkr_helper_t* new_helper = (tkr_helper_t *)calloc(1, sizeof(tkr_helper_t));

    if(tkr_level >= 2) {//write to file
        if(!file_path){
            printf("tkr_helper_init() failed, tracker file path is not set.\n");
            return NULL;
        }
    }

    new_helper->tkr_file_path = strdup(file_path);
    new_helper->tkr_line_format = strdup(tkr_line_format);
    new_helper->tkr_level = tkr_level;
    new_helper->pid = getpid();
    new_helper->tid = pthread_self();

    // update file path with PID
    int prefix_len = snprintf(NULL, 0, "%d_vol-%s", getpid(), file_path);
    new_helper->tkr_file_path = (char*)malloc((prefix_len + 1) * sizeof(char));
    snprintf(new_helper->tkr_file_path, prefix_len + 1, "%d_vol-%s", getpid(), file_path);

    printf("vol new_helper tkr_file_path: %s\n", new_helper->tkr_file_path);

    new_helper->opened_files = NULL;
    new_helper->opened_files_cnt = 0;

    /* VFD vars start */
    // new_helper->vfd_opened_files = NULL;
    // new_helper->vfd_opened_files_cnt = 0;
    new_helper->tracker_page_size = get_tracker_page_size();
    /* VFD vars end */

    getlogin_r(new_helper->user_name, 32);

    // if(new_helper->tkr_level == File_only || new_helper->tkr_level == File_and_print)
    //     new_helper->tkr_file_handle = fopen(new_helper->tkr_file_path, "a");

    // _dic_init();
    return new_helper;
}


datatype_tkr_info_t *new_dtype_info(file_tkr_info_t* root_file,
    const char *name, H5O_token_t token)
{
    datatype_tkr_info_t *info;

    info = (datatype_tkr_info_t *)calloc(1, sizeof(datatype_tkr_info_t));
    info->obj_info.tkr_helper = TKR_HELPER;
    info->obj_info.file_info = root_file;
    // info->obj_info.name = name ? strdup(name) : NULL;
    info->obj_info.name = (char*) malloc(sizeof(char) * (strlen(name) + 1));
    strcpy(info->obj_info.name, name);
    info->obj_info.name = (char*) name;
    info->obj_info.token = token;

    return info;
}

dataset_tkr_info_t *new_dataset_info(file_tkr_info_t *root_file,
    const char *name, H5O_token_t token)
{
    dataset_tkr_info_t *info;

    info = (dataset_tkr_info_t *)calloc(1, sizeof(dataset_tkr_info_t));
    info->obj_info.tkr_helper = TKR_HELPER;
    info->obj_info.file_info = root_file;
    info->obj_info.name = name ? strdup(name) : NULL;
    // info->obj_info.name = (char*) malloc(sizeof(char) * (strlen(name) + 1));
    // strcpy(info->obj_info.name, name);
    // printf("new_dataset_info() name: %s\n", name);
    // printf("new_dataset_info() info->obj_info.name: %s\n", info->obj_info.name);
    info->obj_info.token = token;

    // initialize dset_info values
    info->start_time = get_time_usec();
    info->sorder_id=0;
    info->pfile_sorder_id = 0;
    info->dataset_read_cnt = 0;
    info->dataset_write_cnt = 0;


    info->blob_put_cnt = 0;
    info->total_bytes_blob_put = 0;
    info->total_blob_put_time = 0;
    info->blob_get_cnt = 0;
    info->total_bytes_blob_get = 0;
    info->total_blob_get_time = 0;

    return info;
}

group_tkr_info_t *new_group_info(file_tkr_info_t *root_file,
    const char *name, H5O_token_t token)
{
    group_tkr_info_t *info;

    info = (group_tkr_info_t *)calloc(1, sizeof(group_tkr_info_t));
    info->obj_info.tkr_helper = TKR_HELPER;
    info->obj_info.file_info = root_file;
    // info->obj_info.name = name ? strdup(name) : NULL;
    info->obj_info.name = (char*) malloc(sizeof(char) * (strlen(name) + 1));
    strcpy(info->obj_info.name, name);
    info->obj_info.token = token;

    return info;
}

attribute_tkr_info_t *new_attribute_info(file_tkr_info_t *root_file,
    const char *name, H5O_token_t token)
{
    attribute_tkr_info_t *info;

    info = (attribute_tkr_info_t *)calloc(1, sizeof(attribute_tkr_info_t));
    info->obj_info.tkr_helper = TKR_HELPER;
    info->obj_info.file_info = root_file;
    // info->obj_info.name = name ? strdup(name) : NULL;
    info->obj_info.name = (char*) malloc(sizeof(char) * (strlen(name) + 1));
    strcpy(info->obj_info.name, name);
    info->obj_info.token = token;

    return info;
}

file_tkr_info_t* new_file_info(const char* fname, unsigned long file_no)
{
    file_tkr_info_t *info;

    info = (file_tkr_info_t *)calloc(1, sizeof(file_tkr_info_t));

    info->file_name = fname ? strdup(fname) : NULL;
    // info->file_name = malloc(sizeof(char) * (strlen(fname) + 1));
    // strcpy(info->file_name, fname);
    info->tkr_helper = TKR_HELPER;
    info->file_no = file_no;
    info->sorder_id=0;
    info->ds_created =0;
    info->ds_accessed =0;
    info->grp_created =0;
    info->grp_accessed =0;
    info->dtypes_created =0;
    info->dtypes_accessed =0;

    // // add task name
    // const char* curr_task = getenv("CURR_TASK");
    // info->task_name = curr_task ? strdup(curr_task) : NULL;

    return info;
}

void dtype_info_free(datatype_tkr_info_t* info)
{
    // if(info->obj_info.name)
    //     free(info->obj_info.name);
    free(info);
}

void file_info_free(file_tkr_info_t* info)
{
#ifdef H5_HAVE_PARALLEL
    // Release MPI Comm & Info, if they are valid
    if(info->mpi_comm_info_valid) {
	if(MPI_COMM_NULL != info->mpi_comm)
	    MPI_Comm_free(&info->mpi_comm);
	if(MPI_INFO_NULL != info->mpi_info)
	    MPI_Info_free(&info->mpi_info);
    }
#endif /* H5_HAVE_PARALLEL */
    // if(info->file_name)
    //     free((void*)info->file_name);
    // if(info->task_name)
    //     free((void*)info->task_name);
    free(info);
}

void group_info_free(group_tkr_info_t* info)
{
    // if(info->obj_info.name)
    //     free(info->obj_info.name);
    free(info);
}

void dataset_info_free(dataset_tkr_info_t* info)
{
    // if(info->obj_info.name)
    //     free(info->obj_info.name);

    // if(info->pfile_name)
    //     free(info->pfile_name);
    free(info);
}

void attribute_info_free(attribute_tkr_info_t* info)
{
    // if(info->obj_info.name)
    //     free(info->obj_info.name);
    free(info);
}

datatype_tkr_info_t * add_dtype_node(file_tkr_info_t *file_info,
    H5VL_tracker_t *dtype, const char *obj_name, H5O_token_t token)
{
    unsigned long start = get_time_usec();
    datatype_tkr_info_t *cur;
    int cmp_value;

    assert(file_info);

    // Find datatype in linked list of opened datatypes
    cur = file_info->opened_dtypes;
    while (cur) {
        if (H5VLtoken_cmp(dtype->under_object, dtype->under_vol_id,
		          &(cur->obj_info.token), &token, &cmp_value) < 0)
	    fprintf(stderr, "H5VLtoken_cmp error");
        if (cmp_value == 0)
            break;
        cur = cur->next;
    }

    if(!cur) {
        // Allocate and initialize new datatype node
        cur = new_dtype_info(file_info, obj_name, token);

        // Increment refcount on file info
        file_info->ref_cnt++;

        // Add to linked list
        cur->next = file_info->opened_dtypes;
        file_info->opened_dtypes = cur;
        file_info->opened_dtypes_cnt++;
    }

    // Increment refcount on datatype
    cur->obj_info.ref_cnt++;

    DT_LL_TOTAL_TIME += (get_time_usec() - start);
    return cur;
}

int rm_dtype_node(tkr_helper_t *helper, void *under, hid_t under_vol_id, datatype_tkr_info_t *dtype_info)
{
    unsigned long start = get_time_usec();
    file_tkr_info_t *file_info;
    datatype_tkr_info_t *cur;
    datatype_tkr_info_t *last;
    int cmp_value;

    // Decrement refcount
    dtype_info->obj_info.ref_cnt--;

    // If refcount still >0, leave now
    if(dtype_info->obj_info.ref_cnt > 0)
        return dtype_info->obj_info.ref_cnt;

    // Refcount == 0, remove datatype from file info

    file_info = dtype_info->obj_info.file_info;
    assert(file_info);
    assert(file_info->opened_dtypes);

    cur = file_info->opened_dtypes;
    last = cur;
    while(cur) {
        if (H5VLtoken_cmp(under, under_vol_id, &(cur->obj_info.token),
                          &(dtype_info->obj_info.token), &cmp_value) < 0)
	    fprintf(stderr, "H5VLtoken_cmp error");
        if (cmp_value == 0) {
            //special case: first node is the target, ==cur
            if(cur == file_info->opened_dtypes)
                file_info->opened_dtypes = file_info->opened_dtypes->next;
            else
                last->next = cur->next;

            dtype_info_free(cur);

            file_info->opened_dtypes_cnt--;
            if(file_info->opened_dtypes_cnt == 0)
                assert(file_info->opened_dtypes == NULL);

            // Decrement refcount on file info
            DT_LL_TOTAL_TIME += (get_time_usec() - start);
            rm_file_node(helper, file_info->file_no);

            return 0;
        }

        last = cur;
        cur = cur->next;
    }

    DT_LL_TOTAL_TIME += (get_time_usec() - start);
    //node not found.
    return -1;
}

group_tkr_info_t *add_grp_node(file_tkr_info_t *file_info,
    H5VL_tracker_t *upper_o, const char *obj_name, H5O_token_t token)
{
    group_tkr_info_t *cur;
    unsigned long start = get_time_usec();
    assert(file_info);
    int cmp_value;

    // Find group in linked list of opened groups
    cur = file_info->opened_grps;
    while (cur) {
        if (H5VLtoken_cmp(upper_o->under_object, upper_o->under_vol_id,
                          &(cur->obj_info.token), &token, &cmp_value) < 0)
	    fprintf(stderr, "H5VLtoken_cmp error");
        if (cmp_value == 0)
            break;
        cur = cur->next;
    }

    if(!cur) {
        // Allocate and initialize new group node
        cur = new_group_info(file_info, obj_name, token);

        // Increment refcount on file info
        file_info->ref_cnt++;

        // Add to linked list
        cur->next = file_info->opened_grps;
        file_info->opened_grps = cur;
        file_info->opened_grps_cnt++;
    }

    // Increment refcount on group
    cur->obj_info.ref_cnt++;

    GRP_LL_TOTAL_TIME += (get_time_usec() - start);
    return cur;
}

int rm_grp_node(tkr_helper_t *helper, void *under_obj, hid_t under_vol_id, group_tkr_info_t *grp_info)
{   unsigned long start = get_time_usec();
    file_tkr_info_t *file_info;
    group_tkr_info_t *cur;
    group_tkr_info_t *last;
    int cmp_value;

    // Decrement refcount
    grp_info->obj_info.ref_cnt--;

    // If refcount still >0, leave now
    if(grp_info->obj_info.ref_cnt > 0)
        return grp_info->obj_info.ref_cnt;

    // Refcount == 0, remove group from file info

    file_info = grp_info->obj_info.file_info;
    assert(file_info);
    assert(file_info->opened_grps);

    cur = file_info->opened_grps;
    last = cur;
    while(cur) {
        if (H5VLtoken_cmp(under_obj, under_vol_id, &(cur->obj_info.token),
                          &(grp_info->obj_info.token), &cmp_value) < 0)
	    fprintf(stderr, "H5VLtoken_cmp error");
        if (cmp_value == 0) { //node found
            //special case: first node is the target, ==cur
            if (cur == file_info->opened_grps)
                file_info->opened_grps = file_info->opened_grps->next;
            else
                last->next = cur->next;

            group_info_free(cur);

            file_info->opened_grps_cnt--;
            if (file_info->opened_grps_cnt == 0)
                assert(file_info->opened_grps == NULL);

            // Decrement refcount on file info
            GRP_LL_TOTAL_TIME += (get_time_usec() - start);
            rm_file_node(helper, file_info->file_no);

            return 0;
        }

        last = cur;
        cur = cur->next;
    }

    GRP_LL_TOTAL_TIME += (get_time_usec() - start);
    //node not found.
    return -1;
}

attribute_tkr_info_t *add_attr_node(file_tkr_info_t *file_info,
    H5VL_tracker_t *attr, const char *obj_name, H5O_token_t token)
{   unsigned long start = get_time_usec();
    attribute_tkr_info_t *cur;
    int cmp_value;

    assert(file_info);

    // Find attribute in linked list of opened attributes
    cur = file_info->opened_attrs;
    while (cur) {
        if (H5VLtoken_cmp(attr->under_object, attr->under_vol_id,
                          &(cur->obj_info.token), &token, &cmp_value) < 0)
	    fprintf(stderr, "H5VLtoken_cmp error");
        if (cmp_value == 0)
            break;
        cur = cur->next;
    }

    if(!cur) {
        // Allocate and initialize new attribute node
        cur = new_attribute_info(file_info, obj_name, token);

        // Increment refcount on file info
        file_info->ref_cnt++;

        // Add to linked list
        cur->next = file_info->opened_attrs;
        file_info->opened_attrs = cur;
        file_info->opened_attrs_cnt++;
    }

    // Increment refcount on attribute
    cur->obj_info.ref_cnt++;

    ATTR_LL_TOTAL_TIME += (get_time_usec() - start);
    return cur;
}

int rm_attr_node(tkr_helper_t *helper, void *under_obj, hid_t under_vol_id, attribute_tkr_info_t *attr_info)
{   unsigned long start = get_time_usec();
    file_tkr_info_t *file_info;
    attribute_tkr_info_t *cur;
    attribute_tkr_info_t *last;
    int cmp_value;

    // Decrement refcount
    attr_info->obj_info.ref_cnt--;

    // If refcount still >0, leave now
    if(attr_info->obj_info.ref_cnt > 0)
        return attr_info->obj_info.ref_cnt;

    // Refcount == 0, remove attribute from file info

    file_info = attr_info->obj_info.file_info;
    assert(file_info);
    assert(file_info->opened_attrs);

    cur = file_info->opened_attrs;
    last = cur;
    while(cur) {
	if (H5VLtoken_cmp(under_obj, under_vol_id, &(cur->obj_info.token),
                          &(attr_info->obj_info.token), &cmp_value) < 0)
	    fprintf(stderr, "H5VLtoken_cmp error");
	if (cmp_value == 0) { //node found
            //special case: first node is the target, ==cur
            if(cur == file_info->opened_attrs)
                file_info->opened_attrs = file_info->opened_attrs->next;
            else
                last->next = cur->next;

            attribute_info_free(cur);

            file_info->opened_attrs_cnt--;
            if(file_info->opened_attrs_cnt == 0)
                assert(file_info->opened_attrs == NULL);

            ATTR_LL_TOTAL_TIME += (get_time_usec() - start);

            // Decrement refcount on file info
            rm_file_node(helper, file_info->file_no);

            return 0;
        }

        last = cur;
        cur = cur->next;
    }

    ATTR_LL_TOTAL_TIME += (get_time_usec() - start);
    //node not found.
    return -1;
}

file_tkr_info_t* add_file_node(tkr_helper_t* helper, const char* file_name,
    unsigned long file_no)
{
    unsigned long start = get_time_usec();
    file_tkr_info_t* cur;

    assert(helper);

    if(!helper->opened_files) //empty linked list, no opened file.
        assert(helper->opened_files_cnt == 0);

    // Search for file in list of currently opened ones
    cur = helper->opened_files;
    while (cur) {
        assert(cur->file_no);

        if (cur->file_no == file_no)
            break;

        cur = cur->next;
    }

    if(!cur) {
        // Allocate and initialize new file node
        cur = new_file_info(file_name, file_no);

        // Add to linked list
        cur->next = helper->opened_files;
        helper->opened_files = cur;
        helper->opened_files_cnt++;
    }

    // Increment refcount on file node
    cur->ref_cnt++;
    cur->open_time = get_time_usec();

    tkrLockAcquire(&myLock);
    // TODO(candice): only increment if file is not already open
    FILE_SORDER+=1;
    cur->sorder_id =FILE_SORDER; // sync order with VFD, no add
    tkrLockRelease(&myLock);

    FILE_LL_TOTAL_TIME += (get_time_usec() - start);

    return cur;
}

//need a dumy node to make it simpler
int rm_file_node(tkr_helper_t* helper, unsigned long file_no)
{
    unsigned long start = get_time_usec();
    file_tkr_info_t* cur;
    file_tkr_info_t* last;

    assert(helper);
    assert(helper->opened_files);
    assert(helper->opened_files_cnt);
    assert(file_no);

    cur = helper->opened_files;
    last = cur;
    while(cur) {
        // Node found
        if(cur->file_no == file_no) {
            // Decrement file node's refcount
            cur->ref_cnt--;

            // If refcount == 0, remove file node & maybe print file stats
            if(cur->ref_cnt == 0) {
                // Sanity checks
                assert(0 == cur->opened_datasets_cnt);
                assert(0 == cur->opened_grps_cnt);
                assert(0 == cur->opened_dtypes_cnt);
                assert(0 == cur->opened_attrs_cnt);

                // Unlink from list of opened files
                if(cur == helper->opened_files) //first node is the target
                    helper->opened_files = helper->opened_files->next;
                else
                    last->next = cur->next;

                // Free file info
                file_info_free(cur);

                // Update connector info
                helper->opened_files_cnt--;
                if(helper->opened_files_cnt == 0)
                    assert(helper->opened_files == NULL);
            }

            break;
        }

        // Advance to next file node
        last = cur;
        cur = cur->next;
    }

    FILE_LL_TOTAL_TIME += (get_time_usec() - start);
    return helper->opened_files_cnt;
}

file_tkr_info_t* _search_home_file(unsigned long obj_file_no){
    file_tkr_info_t* cur;

    if(TKR_HELPER->opened_files_cnt < 1)
        return NULL;

    cur = TKR_HELPER->opened_files;
    while (cur) {
        if (cur->file_no == obj_file_no) {//file found
            cur->ref_cnt++;
            return cur;
        }

        cur = cur->next;
    }

    return NULL;
}

dataset_tkr_info_t * add_dataset_node(unsigned long obj_file_no,
    H5VL_tracker_t *dset, H5O_token_t token,
    file_tkr_info_t *file_info_in, const char* ds_name,
    hid_t dxpl_id, void** req)
{
    unsigned long start = get_time_usec();
    file_tkr_info_t* file_info;
    dataset_tkr_info_t* cur;
    int cmp_value;

    assert(dset);
    assert(dset->under_object);
    assert(file_info_in);
	
    if (obj_file_no != file_info_in->file_no) {//creating a dataset from an external place
        file_tkr_info_t* external_home_file;

        external_home_file = _search_home_file(obj_file_no);
        if(external_home_file){//use extern home
            file_info = external_home_file;
        }else{//extern home not exist, fake one
            file_info = new_file_info("dummy", obj_file_no);
        }
    }else{//local
        file_info = file_info_in;
    }

    // Find dataset in linked list of opened datasets
    cur = file_info->opened_datasets;
    while (cur) {
        if (H5VLtoken_cmp(dset->under_object, dset->under_vol_id,
                          &(cur->obj_info.token), &token, &cmp_value) < 0)
	    fprintf(stderr, "H5VLtoken_cmp error");
        if (cmp_value == 0)
	    break;

        cur = cur->next;
    }

    if(!cur) {
        cur = new_ds_tkr_info(dset->under_object, dset->under_vol_id, token, file_info, ds_name, dxpl_id, req);

        // Increment refcount on file info
        file_info->ref_cnt++;

        // Add to linked list of opened datasets
        cur->next = file_info->opened_datasets;
        file_info->opened_datasets = cur;
        file_info->opened_datasets_cnt++;
    }
    // print to check file_info->file_name
    /* Add dset info that requires parent file info */
    cur->pfile_name = file_info->file_name ? strdup(file_info->file_name) : NULL;
    // TODO: update meta_page to tracking object
    cur->metadata_file_page = NULL;
    cur->metadata_file_page = (size_t) malloc(sizeof(size_t));
    cur->metadata_file_page = token_to_num(cur->obj_info.token) / TKR_HELPER->tracker_page_size;

    tkrLockAcquire(&myLock);
    cur->sorder_id = ++DATA_SORDER;
    tkrLockRelease(&myLock);

    // Increment refcount on dataset
    cur->obj_info.ref_cnt++;

    DS_LL_TOTAL_TIME += (get_time_usec() - start);
    return cur;
}


//need a dumy node to make it simpler
int rm_dataset_node(tkr_helper_t *helper, void *under_obj, hid_t under_vol_id, dataset_tkr_info_t *dset_info)
{
    unsigned long start = get_time_usec();
    file_tkr_info_t *file_info;
    dataset_tkr_info_t *cur;
    dataset_tkr_info_t *last;
    int cmp_value;

    // Decrement refcount
    dset_info->obj_info.ref_cnt--;

    // If refcount still >0, leave now
    if(dset_info->obj_info.ref_cnt > 0)
        return dset_info->obj_info.ref_cnt;

    // Refcount == 0, remove dataset from file info
    file_info = dset_info->obj_info.file_info;
    assert(file_info);
    assert(file_info->opened_datasets);

    cur = file_info->opened_datasets;
    last = cur;
    while(cur){
        if (H5VLtoken_cmp(under_obj, under_vol_id, &(cur->obj_info.token),
                          &(dset_info->obj_info.token), &cmp_value) < 0)
	    fprintf(stderr, "H5VLtoken_cmp error");
	if (cmp_value == 0) {//node found
            //special case: first node is the target, ==cur
            if(cur == file_info->opened_datasets)
                file_info->opened_datasets = file_info->opened_datasets->next;
            else
                last->next = cur->next;

            dataset_info_free(cur);

            file_info->opened_datasets_cnt--;
            if(file_info->opened_datasets_cnt == 0)
                assert(file_info->opened_datasets == NULL);

            // Decrement refcount on file info
            DS_LL_TOTAL_TIME += (get_time_usec() - start);
            rm_file_node(helper, file_info->file_no);

            return 0;
        }

        last = cur;
        cur = cur->next;
    }

    DS_LL_TOTAL_TIME += (get_time_usec() - start);
    //node not found.
    return -1;
}

void ptr_cnt_increment(tkr_helper_t* helper){
    assert(helper);

    //mutex lock

    if(helper){
        (helper->ptr_cnt)++;
    }

    //mutex unlock
}

void ptr_cnt_decrement(tkr_helper_t* helper){
    assert(helper);

    //mutex lock

    helper->ptr_cnt--;

    //mutex unlock

    if(helper->ptr_cnt == 0){
        // do nothing for now.
        //tkr_helper_teardown(helper);loggin is not decided yet.
    }
}


void get_time_str(char *str_out){
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    *str_out = '\0';
    sprintf(str_out, "%d/%d/%d %d:%d:%d", timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

dataset_tkr_info_t * new_ds_tkr_info(void* under_object, hid_t vol_id, H5O_token_t token,
        file_tkr_info_t* file_info, const char* ds_name, hid_t dxpl_id, void **req){
    hid_t dcpl_id = -1;
    hid_t dt_id = -1;
    hid_t ds_id = -1;
    dataset_tkr_info_t* ds_info;

    assert(under_object);
    assert(file_info);

    ds_info = new_dataset_info(file_info, ds_name, token);

    dt_id = dataset_get_type(under_object, vol_id, dxpl_id);
    ds_info->dt_class = H5Tget_class(dt_id);
    ds_info->dset_type_size = H5Tget_size(dt_id);
    H5Tclose(dt_id);

    ds_id = dataset_get_space(under_object, vol_id, dxpl_id);
    if (ds_info->ds_class == H5S_SIMPLE) {
        // dimension_cnt, dimensions, and dset_n_elements are not ready here
        ds_info->dimension_cnt = (unsigned)H5Sget_simple_extent_ndims(ds_id);
        H5Sget_simple_extent_dims(ds_id, ds_info->dimensions, NULL);
        ds_info->dset_n_elements = (hsize_t)H5Sget_simple_extent_npoints(ds_id);

        ds_info->ds_class = H5Sget_simple_extent_type(ds_id);
    }
    // ds_info->dset_offset = H5Dget_offset(ds_id); // TODO: all returns -1
    H5Sclose(ds_id);

    // dcpl_id = dataset_get_dcpl(under_object, vol_id, dxpl_id);
    // H5Pclose(dcpl_id);

    return ds_info;
}

void _new_loc_pram(H5I_type_t type, H5VL_loc_params_t *lparam)
{
    assert(lparam);

    lparam->type = H5VL_OBJECT_BY_SELF;
    lparam->obj_type = type;
    return;
}

static int get_native_info(void *obj, H5I_type_t target_obj_type, hid_t connector_id,
                       hid_t dxpl_id, H5O_info2_t *oinfo)
{
    H5VL_object_get_args_t vol_cb_args; /* Arguments to VOL callback */
    H5VL_loc_params_t loc_params;

    /* Set up location parameter */
    _new_loc_pram(target_obj_type, &loc_params);

    /* Set up VOL callback arguments */
    vol_cb_args.op_type              = H5VL_OBJECT_GET_INFO;
    vol_cb_args.args.get_info.oinfo  = oinfo;
    vol_cb_args.args.get_info.fields = H5O_INFO_BASIC;

    if(H5VLobject_get(obj, &loc_params, connector_id, &vol_cb_args, dxpl_id, NULL) < 0)
        return -1;

    return 0;
}


// TODO: need to check update
static int get_native_file_no(const H5VL_tracker_t *file_obj, unsigned long *fileno)
{
    H5VL_file_get_args_t vol_cb_args; /* Arguments to VOL callback */

    /* Set up VOL callback arguments */
    vol_cb_args.op_type              = H5VL_FILE_GET_FILENO;
    vol_cb_args.args.get_fileno.fileno = fileno;

    if(H5VLfile_get(file_obj->under_object, file_obj->under_vol_id, &vol_cb_args, H5P_DEFAULT, NULL) < 0)
        return -1;

    return 0;
}

H5VL_tracker_t *_file_open_common(void *under, hid_t vol_id,
    const char *name)
{
    H5VL_tracker_t *file;
    unsigned long file_no = 0;

    file = H5VL_tracker_new_obj(under, vol_id, TKR_HELPER);
    file->my_type = H5I_FILE;
    get_native_file_no(file, &file_no);
    file->generic_tkr_info = add_file_node(TKR_HELPER, name, file_no);

    return file;
}

herr_t tracker_file_setup(const char* str_in, char* file_path_out, Track_level* level_out, char* format_out){
    //acceptable format: path=$path_str;level=$level_int;format=$format_str
    char tmp_str[100] = {'\0'};
    char* toklist[4] = {NULL};
    int i;
    char *p;

    memcpy(tmp_str, str_in, strlen(str_in)+1);

    i = 0;
    p = strtok(tmp_str, ";");
    while(p != NULL) {
        toklist[i] = strdup(p);
        p = strtok(NULL, ";");
        i++;
    }

    sscanf(toklist[1], "path=%s", file_path_out);
    sscanf(toklist[2], "level=%d", (int *)level_out);
    sscanf(toklist[3], "format=%s", format_out);

    for(i = 0; i<=3; i++)
        if(toklist[i])
            free(toklist[i]);

    return 0;
}

//This function makes up a fake upper layer obj used as a parameter in _obj_wrap_under(..., H5VL_tracker_t* upper_o,... ),
//Use this in H5VL_tracker_wrap_object() ONLY!!!
H5VL_tracker_t * _fake_obj_new(file_tkr_info_t *root_file, hid_t under_vol_id)
{
    H5VL_tracker_t* obj;

    obj = H5VL_tracker_new_obj(NULL, under_vol_id, TKR_HELPER);
    obj->my_type = H5I_FILE;  // FILE should work fine as a parent obj for all.
    obj->generic_tkr_info = (void*)root_file;

    return obj;
}

void _fake_obj_free(H5VL_tracker_t *obj)
{
    H5VL_tracker_free_obj(obj);
}


/* under: obj need to be wrapped
 * upper_o: holder or upper layer object. Mostly used to pass root_file_info, vol_id, etc,.
 *      - it's a fake obj if called by H5VL_tracker_wrap_object().
 * target_obj_type:
 *      - for H5VL_tracker_wrap_object(obj_type): the obj should be wrapped into this type
 *      - for H5VL_tracker_object_open(): it's the obj need to be opened as this type
 *
 */
H5VL_tracker_t * _obj_wrap_under(void *under, H5VL_tracker_t *upper_o,
                                    const char *target_obj_name,
                                    H5I_type_t target_obj_type,
                                    hid_t dxpl_id, void **req)
{
    H5VL_tracker_t *obj;
    file_tkr_info_t *file_info = NULL;

    if (under) {
        H5O_info2_t oinfo;
        H5O_token_t token;
        unsigned long file_no;

        //open from types
        switch(upper_o->my_type) {
            case H5I_DATASET:
            case H5I_GROUP:
            case H5I_DATATYPE:
            case H5I_ATTR:
                file_info = ((object_tkr_info_t *)(upper_o->generic_tkr_info))->file_info;
                break;

            case H5I_FILE:
                file_info = (file_tkr_info_t*)upper_o->generic_tkr_info;
                break;

            case H5I_UNINIT:
            case H5I_BADID:
            case H5I_DATASPACE:
            case H5I_VFL:
            case H5I_VOL:
            case H5I_GENPROP_CLS:
            case H5I_GENPROP_LST:
            case H5I_ERROR_CLASS:
            case H5I_ERROR_MSG:
            case H5I_ERROR_STACK:
            case H5I_NTYPES:
            default:
                file_info = NULL;  // Error
                break;
        }
        assert(file_info);

        obj = H5VL_tracker_new_obj(under, upper_o->under_vol_id, upper_o->tkr_helper);

        /* Check for async request */
        if (req && *req)
            *req = H5VL_tracker_new_obj(*req, upper_o->under_vol_id, upper_o->tkr_helper);

        //obj types
        if(target_obj_type != H5I_FILE) {
            // Sanity check
            assert(target_obj_type == H5I_DATASET || target_obj_type == H5I_GROUP ||
                    target_obj_type == H5I_DATATYPE || target_obj_type == H5I_ATTR);

            get_native_info(under, target_obj_type, upper_o->under_vol_id,
                            dxpl_id, &oinfo);
            token = oinfo.token;
            file_no = oinfo.fileno;
        }
        else
            get_native_file_no(obj, &file_no);

        switch (target_obj_type) {
            case H5I_DATASET:
                obj->generic_tkr_info = add_dataset_node(file_no, obj, token, file_info, target_obj_name, dxpl_id, req);
                obj->my_type = H5I_DATASET;

                // file_ds_created(file_info); //candice added
                file_ds_accessed(file_info);
                break;

            case H5I_GROUP:
                obj->generic_tkr_info = add_grp_node(file_info, obj, target_obj_name, token);
                obj->my_type = H5I_GROUP;
                break;

            case H5I_FILE: //newly added. if target_obj_name == NULL: it's a fake upper_o
                obj->generic_tkr_info = add_file_node(TKR_HELPER, target_obj_name, file_no);
                obj->my_type = H5I_FILE;
                break;

            case H5I_DATATYPE:
                obj->generic_tkr_info = add_dtype_node(file_info, obj, target_obj_name, token);
                obj->my_type = H5I_DATATYPE;
                break;

            case H5I_ATTR:
                obj->generic_tkr_info = add_attr_node(file_info, obj, target_obj_name, token);
                obj->my_type = H5I_ATTR;
                break;

            case H5I_UNINIT:
            case H5I_BADID:
            case H5I_DATASPACE:
            case H5I_VFL:
            case H5I_VOL:
                /* TODO(candice): this is redundant */
                // obj->generic_tkr_info = add_dataset_node(file_no, obj, token, file_info, target_obj_name, dxpl_id, req);
                // obj->my_type = H5I_VOL;

                // file_ds_created(file_info); //candice added
                // file_ds_accessed(file_info);
                break;
            case H5I_GENPROP_CLS:
            case H5I_GENPROP_LST:
            case H5I_ERROR_CLASS:
            case H5I_ERROR_MSG:
            case H5I_ERROR_STACK:
            case H5I_NTYPES:
            default:
                break;
        }
    } /* end if */
    else
        obj = NULL;

    return obj;
}

unsigned int genHash(const char *msg) {
    unsigned long hash = 0;
    unsigned long c;
    unsigned int func_index;
    const char* tmp = msg;

    while (0 != (c = (unsigned long)(*msg++))) {//SDBM hash
        hash = c + (hash << 6) + (hash << 16) - hash;
    }

    msg = tmp;//restore string head address
    func_index = (unsigned int)(hash % STAT_FUNC_MOD);
    if(!FUNC_DIC[func_index]) {
        FUNC_DIC[func_index] = strdup(msg);
        //printf("received msg = %s, hash index = %d, result msg = %s\n", msg, func_index, FUNC_DIC[func_index]);
    }

    return func_index;
}

void _dic_init(void){
    for(int i = 0; i < STAT_FUNC_MOD; i++){
        FUNC_DIC[i] = NULL;
    }
}

void _dic_free(void){
    for(int i = 0; i < STAT_FUNC_MOD; i++){
        if(FUNC_DIC[i]){
            free(FUNC_DIC[i]);
        }
    }
}

// TODO: currently not used
void tkr_verify_open_things(int open_files, int open_dsets)
{
    if(TKR_HELPER) {
        assert(open_files == TKR_HELPER->opened_files_cnt);

        /* Check opened datasets */
        if(open_files > 0) {
            file_tkr_info_t* opened_file;
            int total_open_dsets = 0;

            opened_file = TKR_HELPER->opened_files;
            while(opened_file) {
                total_open_dsets += opened_file->opened_datasets_cnt;
                opened_file = opened_file->next;
            }
            assert(open_dsets == total_open_dsets);
        }
    }
}

// TODO: need to be fixed if the function got called
void tkr_log_open_things(FILE *f)
{
    if(TKR_HELPER) {
        file_tkr_info_t * opened_file;
        unsigned file_count = 0;

        fprintf(f, "# of open files: %d\n", TKR_HELPER->opened_files_cnt);

        /* Print opened files */
        opened_file = TKR_HELPER->opened_files;
        while(opened_file) {
            dataset_tkr_info_t *opened_dataset;
            unsigned dset_count = 0;

            fprintf(f, "file #%u: info ptr = %p, name = '%s', fileno = %llu\n", file_count, (void *)opened_file, opened_file->file_name, opened_file->file_no);
            fprintf(f, "\tref_cnt = %d\n", opened_file->ref_cnt);

            /* Print opened datasets */
            fprintf(f, "\topened_datasets_cnt = %d\n", opened_file->opened_datasets_cnt);
            opened_dataset = opened_file->opened_datasets;
            while(opened_dataset) {
                // need to be fixed if the function got called
                // fprintf(f, "\tdataset #%u: name = '%s', objno = %llu\n", dset_count, opened_dataset->obj_info.name, (unsigned long long)opened_dataset->obj_info.objno);
                fprintf(f, "\tdataset #%u: name = '%s'\n", dset_count, opened_dataset->obj_info.name);
                fprintf(f, "\t\tfile_info ptr = %p\n", (void *)opened_dataset->obj_info.file_info);
                fprintf(f, "\t\tref_cnt = %d\n", opened_dataset->obj_info.ref_cnt);

                dset_count++;
                opened_dataset = opened_dataset->next;
            }

            fprintf(f, "\topened_grps_cnt = %d\n", opened_file->opened_grps_cnt);
            fprintf(f, "\topened_dtypes_cnt = %d\n", opened_file->opened_dtypes_cnt);
            fprintf(f, "\topened_attrs_cnt = %d\n", opened_file->opened_attrs_cnt);

            file_count++;
            opened_file = opened_file->next;
        }
    }
    else
        fprintf(f, "TKR_HELPER not initialized\n");
}























/* Tracker internal print and logs implementation */

void _dic_print(void){
    for(int i = 0; i < STAT_FUNC_MOD; i++){
        if(FUNC_DIC[i]){
            printf("%d %s\n", i, FUNC_DIC[i]);
        }
    }
}
void _preset_dic_print(void){
    const char* preset_dic[] = {
            "H5VL_tracker_init",                         /* initialize   */
            "H5VL_tracker_term",                         /* terminate    */
            "H5VL_tracker_info_copy",                /* info copy    */
            "H5VL_tracker_info_cmp",                 /* info compare */
            "H5VL_tracker_info_free",                /* info free    */
            "H5VL_tracker_info_to_str",              /* info to str  */
            "H5VL_tracker_str_to_info",              /* str to info  */
            "H5VL_tracker_get_object",               /* get_object   */
            "H5VL_tracker_get_wrap_ctx",             /* get_wrap_ctx */
            "H5VL_tracker_wrap_object",              /* wrap_object  */
            "H5VL_tracker_unwrap_object",            /* unwrap_object  */
            "H5VL_tracker_free_wrap_ctx",            /* free_wrap_ctx */
            "H5VL_tracker_attr_create",                       /* create */
            "H5VL_tracker_attr_open",                         /* open */
            "H5VL_tracker_attr_read",                         /* read */
            "H5VL_tracker_attr_write",                        /* write */
            "H5VL_tracker_attr_get",                          /* get */
            "H5VL_tracker_attr_specific",                     /* specific */
            "H5VL_tracker_attr_optional",                     /* optional */
            "H5VL_tracker_attr_close",                         /* close */
            "H5VL_tracker_dataset_create",                    /* create */
            "H5VL_tracker_dataset_open",                      /* open */
            "H5VL_tracker_dataset_read",                      /* read */
            "H5VL_tracker_dataset_write",                     /* write */
            "H5VL_tracker_dataset_get",                       /* get */
            "H5VL_tracker_dataset_specific",                  /* specific */
            "H5VL_tracker_dataset_optional",                  /* optional */
            "H5VL_tracker_dataset_close",                      /* close */
            "H5VL_tracker_datatype_commit",                   /* commit */
            "H5VL_tracker_datatype_open",                     /* open */
            "H5VL_tracker_datatype_get",                      /* get_size */
            "H5VL_tracker_datatype_specific",                 /* specific */
            "H5VL_tracker_datatype_optional",                 /* optional */
            "H5VL_tracker_datatype_close",                     /* close */
            "H5VL_tracker_file_create",                       /* create */
            "H5VL_tracker_file_open",                         /* open */
            "H5VL_tracker_file_get",                          /* get */
            "H5VL_tracker_file_specific",                     /* specific */
            "H5VL_tracker_file_optional",                     /* optional */
            "H5VL_tracker_file_close",                         /* close */
            "H5VL_tracker_group_create",                      /* create */
            "H5VL_tracker_group_open",                        /* open */
            "H5VL_tracker_group_get",                         /* get */
            "H5VL_tracker_group_specific",                    /* specific */
            "H5VL_tracker_group_optional",                    /* optional */
            "H5VL_tracker_group_close",                        /* close */
            "H5VL_tracker_link_create",                       /* create */
            "H5VL_tracker_link_copy",                         /* copy */
            "H5VL_tracker_link_move",                         /* move */
            "H5VL_tracker_link_get",                          /* get */
            "H5VL_tracker_link_specific",                     /* specific */
            "H5VL_tracker_link_optional",                     /* optional */
            "H5VL_tracker_object_open",                       /* open */
            "H5VL_tracker_object_copy",                       /* copy */
            "H5VL_tracker_object_get",                        /* get */
            "H5VL_tracker_object_specific",                   /* specific */
            "H5VL_tracker_object_optional",                   /* optional */
            "H5VL_tracker_request_wait",                      /* wait */
            "H5VL_tracker_request_notify",
            "H5VL_tracker_request_cancel",
            "H5VL_tracker_request_specific",
            "H5VL_tracker_request_optional",
            "H5VL_tracker_request_free",
            "H5VL_tracker_blob_put",
            "H5VL_tracker_blob_get",
            "H5VL_tracker_blob_specific",
    };
    int size = sizeof(preset_dic) / sizeof(const char*);
    int key_space[1000];

    for(int i = 0; i < 1000; i++){
        key_space[i] = -1;
    }

    for(int i = 0; i < size; i++){
        printf("%d %s\n", genHash(preset_dic[i]), preset_dic[i]);
        if(key_space[genHash(preset_dic[i])] == -1){
            key_space[genHash(preset_dic[i])] = (int)genHash(preset_dic[i]);
        }else
            printf("Collision found: key = %d, hash index = %d\n", key_space[genHash(preset_dic[i])], genHash(preset_dic[i]));
    }
}

//not file_tkr_info_t!
void file_stats_tkr_write(const file_tkr_info_t* file_info) {
    if(!file_info){
       printf("file_stats_tkr_write(): ds_info is NULL.\n");
        return;
    }

    printf("File Close Statistic Summary Start ===========================================\n");
    printf("File name = %s \n",file_info->file_name);
    printf("File order_id = %ld\n",file_info->sorder_id);
    printf("H5 file closed, %d datasets are created, %d datasets are accessed.\n", file_info->ds_created, file_info->ds_accessed);
    printf("File Close Statistic Summary End =============================================\n");

    // tkr_log_open_things(TKR_HELPER->tkr_file_handle);
}

void dataset_stats_tkr_write(const dataset_tkr_info_t* dset_info){

    if(!dset_info){
        printf("dataset_stats_tkr_write(): dset_info is NULL.\n");
        return;
    }

    printf("Dataset Close Statistic Summary Start ===========================================\n");
    printf("Dataset name = %s \n",dset_info->obj_info.name);
    printf("Dataset order_id = %ld-%ld \n",dset_info->pfile_sorder_id,dset_info->sorder_id);
    printf("Dataset parent file name = %s \n",dset_info->pfile_name);
    printf("Dataset type class = %d \n",dset_info->dt_class);
    printf("Dataset type size = %ld \n",dset_info->dset_type_size);
    printf("Dataset space class = %d \n",dset_info->ds_class);
    printf("Dataset storage size = %ld \n",dset_info->storage_size);
    printf("Dataset num elements = %ld \n",dset_info->dset_n_elements);
    printf("Dataset dimension count = %u \n", dset_info->dimension_cnt);
    // print dimentions
    printf("Dataset dimensions = {");
    for (int i=0; i < dset_info->dimension_cnt; i++){
    printf("%ld,",dset_info->dimensions[i]);
    }
    printf("}\n");
    printf("Dataset num hyperslab blocks = %ld \n",dset_info->hyper_nblocks);
    printf("Dataset offset = %ld \n", dset_info->dset_offset);
    // printf("Dataset is read %d time, %lu bytes in total, costs %lu us.\n", dset_info->dataset_read_cnt, dset_info->total_bytes_read, dset_info->total_read_time);
    // printf("Dataset is written %d time, %lu bytes in total, costs %lu us.\n", dset_info->dataset_write_cnt, dset_info->total_bytes_written, dset_info->total_write_time);
    printf("Data Blob is get %d time, %lu bytes in total, costs %lu us.\n", dset_info->blob_get_cnt, dset_info->total_bytes_blob_get, dset_info->total_blob_get_time);
    printf("Data Blob is put %d time, %lu bytes in total, costs %lu us.\n", dset_info->blob_put_cnt, dset_info->total_bytes_blob_put, dset_info->total_blob_put_time);
    printf("Dataset Close Statistic Summary End =============================================\n");

    // tkr_log_open_things(TKR_HELPER->tkr_file_handle);
    
}



void datatype_stats_tkr_write(const datatype_tkr_info_t* dt_info) {
    if(!dt_info){
        //printf("datatype_stats_tkr_write(): ds_info is NULL.\n");
        return;
    }
    //printf("Datatype name = %s, commited %d times, datatype get is called %d times.\n", dt_info->dtype_name, dt_info->datatype_commit_cnt, dt_info->datatype_get_cnt);
}

void group_stats_tkr_write(const group_tkr_info_t* grp_info) {
    if(!grp_info){
        //printf("group_stats_tkr_write(): grp_info is NULL.\n");
        return;
    }
    //printf("group_stats_tkr_write() is yet to be implemented.\n");
}

void attribute_stats_tkr_write(const attribute_tkr_info_t *attr_info) {
    if(!attr_info){
        //printf("attribute_stats_tkr_write(): attr_info is NULL.\n");
        return;
    }
    //printf("attribute_stats_tkr_write() is yet to be implemented.\n");
}

void tkr_helper_teardown(tkr_helper_t* helper){
    if(helper){// not null

#ifdef TRACKER_PROV_LOGGING
        char pline[512];
        FILE * file_handle = fopen(helper->tkr_file_path, "a");

        sprintf(pline,
                "TOTAL_TKR_OVERHEAD %llu\n"
                "TOTAL_NATIVE_H5_TIME %llu\n"
                "TKR_WRITE_TOTAL_TIME %llu\n"
                "FILE_LL_TOTAL_TIME %llu\n"
                "DS_LL_TOTAL_TIME %llu\n"
                "GRP_LL_TOTAL_TIME %llu\n"
                "DT_LL_TOTAL_TIME %llu\n"
                "ATTR_LL_TOTAL_TIME %llu\n",
                TOTAL_TKR_OVERHEAD,
                TOTAL_NATIVE_H5_TIME,
                TKR_WRITE_TOTAL_TIME,
                FILE_LL_TOTAL_TIME,
                DS_LL_TOTAL_TIME,
                GRP_LL_TOTAL_TIME,
                DT_LL_TOTAL_TIME,
                ATTR_LL_TOTAL_TIME);

        switch(helper->tkr_level){
            case File_only:
                fputs(pline, file_handle);
                break;

            case File_and_print:
                fputs(pline, file_handle);
                printf("%s", pline);
                break;

            case Print_only:
                printf("%s", pline);
                break;

            case Level4:
            case Level5:
            case Disabled:
            case Default:
            default:
                break;
        }

        fflush(file_handle);
        fclose(file_handle);
#endif
        tkrLockDestroy(&myLock);
        destroy_hash_lock();

        // if(helper->tkr_level == File_only 
        //     || helper->tkr_level ==File_and_print){//no file
        //     fflush(helper->tkr_file_handle);
        //     fclose(helper->tkr_file_handle);
        // }

        // if(helper->tkr_file_path)
        //     free(helper->tkr_file_path);
        // if(helper->tkr_line_format)
        //     free(helper->tkr_line_format);

        // free(helper);
        // _dic_free();
    }
}


int tkr_write(tkr_helper_t* helper_in, const char* msg, unsigned long duration){
//    assert(strcmp(msg, "root_file_info"));
    unsigned long start = get_time_usec();
    const char* base = "H5VL_tracker_";
    size_t base_len;
    size_t msg_len;
    char time[64];
    char pline[512];

    // assert(helper_in); // This causing segfault with PyFLEXTRKR

    get_time_str(time);

    /* Trimming long VOL function names */
    base_len = strlen(base);
    msg_len = strlen(msg);
    if(msg_len > base_len) {//strlen(H5VL_tracker_) == 16.
        size_t i = 0;

        for(; i < base_len; i++)
            if(base[i] != msg[i])
                break;
    }
#ifdef TRACKER_PROV_LOGGING
    FILE * file_handle = fopen(helper_in->tkr_file_path, "a");

    sprintf(pline, "%s %llu(us)\n",  msg, duration);//assume less than 64 functions

    //printf("Func name:[%s], hash index = [%u], overhead = [%llu]\n",  msg, genHash(msg), duration);
    switch(helper_in->tkr_level){
        case File_only:
            fputs(pline, file_handle);
            break;

        case File_and_print:
            fputs(pline, file_handle);
            printf("%s", pline);
            break;

        case Print_only:
            printf("%s", pline);
            break;

        case Level4:
        case Level5:
        case Disabled:
        case Default:
        default:
            break;
    }

    if(helper_in->tkr_level == (File_only | File_and_print )){
        fputs(pline, file_handle);
    }
//    unsigned tmp = TKR_WRITE_TOTAL_TIME;
    fflush(file_handle); // TODO: check if this is necessary
    fclose(file_handle);
#endif


    TKR_WRITE_TOTAL_TIME += (get_time_usec() - start);
    return 0;
}



    /* candice added routine implementation start*/
void log_file_stat_yaml(tkr_helper_t* helper_in, const file_tkr_info_t* file_info)
{
    FILE * f = fopen(helper_in->tkr_file_path, "a");
    
    if (!file_info) {
        fprintf(f, "log_file_stat_yaml(): file_info is NULL.\n");
        return;
    }

    log_dset_ht_yaml(f);

    // char* file_name = strrchr(file_info->file_name, '/');
    char* file_name = (char *) file_info->file_name;

    if(file_name)
        file_name++;
    else
        file_name = (char*)file_info->file_name;

    fprintf(f, "- file-%ld:\n", file_info->sorder_id);
    fprintf(f, "    file_name: \"/%s\"\n", file_name);
    // if(file_info->task_name != NULL){
    //     fprintf(f, "    task_name: \"%s\"\n", file_info->task_name);
    // }else{
    //     const char* curr_task = getenv("CURR_TASK");
    //     if (curr_task)
    //         fprintf(f, "    task_name: \"%s\"\n", curr_task);
    //     else
    //         fprintf(f, "    task_name: \"\"\n");
    // }
    fprintf(f, "    open_time: %ld\n", file_info->open_time);
    fprintf(f, "    close_time: %ld\n", get_time_usec());
    fprintf(f, "    file_size: %zu\n", file_info->file_size);
    fprintf(f, "    header_size: %zu\n", file_info->header_size);
    fprintf(f, "    sieve_buf_size: %zu\n", file_info->sieve_buf_size);
    fprintf(f, "    file_intent: [\"%s\"]\n", file_info->intent);


    fprintf(f, "    ds_created: %d\n", file_info->ds_created);
    fprintf(f, "    ds_accessed: %d\n", file_info->ds_accessed);
    fprintf(f, "    grp_created: %d\n", file_info->grp_created);
    fprintf(f, "    grp_accessed: %d\n", file_info->grp_accessed);
    fprintf(f, "    dtypes_created: %d\n", file_info->dtypes_created);
    fprintf(f, "    dtypes_accessed: %d\n", file_info->dtypes_accessed);


    fprintf(f, "- Task:\n");
    fprintf(f, "  task_id: %d\n", getpid());
    fprintf(f, "  VOL-Total-Overhead(ms): %ld\n", TOTAL_TKR_OVERHEAD/1000);

    fflush(f);
    fclose(f);
}

void H5VL_arrow_get_selected_sub_region(hid_t space_id, size_t org_type_size) {
    // from Jie's code

    size_t type_size = org_type_size;
    int32_t s_row_idx;
    int32_t s_col_idx;
    int32_t sub_cols;

    // 1. get the memspace dimensional information
    // subRegionInfo->type_size = type_size;
    hsize_t g_sdim[H5S_MAX_RANK];
    int sranks = H5Sget_simple_extent_dims(space_id, g_sdim, NULL);
    int32_t g_rows = 1;
    for (int i = 0; i < sranks - 1; ++i) {
        g_rows *= g_sdim[i];
    }
    int32_t g_cols = g_sdim[sranks - 1];

    // 2. get the start coordinate of the selected region. Here we only consider a single contiguous region
    H5S_sel_type stype = H5Sget_select_type(space_id);
    s_row_idx = 0;
    s_col_idx = 0;

    // if (stype == H5S_SEL_ALL) {
    //     // subRegionInfo->s_row_idx = 0;
    //     // subRegionInfo->s_col_idx = 0;
    //     s_row_idx = 0;
    //     s_col_idx = 0;
    // }
    // else if (stype == H5S_SEL_HYPERSLABS) {
    //     hid_t sel_iter_id = H5Ssel_iter_create(space_id, (size_t)type_size, 0);
    //     size_t nseq;
    //     size_t nelem;
    //     hsize_t seq_start_off;
    //     size_t seq_len;
    //     H5Ssel_iter_get_seq_list(sel_iter_id, 1, (size_t)type_size, &nseq, &nelem, &seq_start_off, &seq_len);
    //     H5Ssel_iter_close(sel_iter_id);

    //     // 3. get the start coordinate according to the start_offset
    //     // subRegionInfo->s_row_idx = (seq_start_off / type_size) / g_cols;
    //     // subRegionInfo->s_col_idx = (seq_start_off / type_size) % g_cols;
    //     s_row_idx = (seq_start_off / type_size) / g_cols;
    //     s_col_idx = (seq_start_off / type_size) % g_cols;
    // }

    
    hid_t sel_iter_id = H5Ssel_iter_create(space_id, (size_t)type_size, 0);
    size_t nseq;
    size_t nelem;
    int ndim = H5Sget_simple_extent_ndims(space_id);
    hsize_t * seq_start_off = (hsize_t *)malloc(sizeof(hsize_t) * ndim);

    // uint64_t *seq_start_off = (uint64_t *)malloc(sizeof(uint64_t) * ndim);

    size_t seq_len;
    // H5Ssel_iter_get_seq_list(sel_iter_id, 1, (size_t)type_size, &nseq, &nelem, seq_start_off, &seq_len);
    H5Ssel_iter_get_seq_list(sel_iter_id, 1, (size_t)type_size, &nseq, &nelem, seq_start_off, &seq_len);


    H5Ssel_iter_close(sel_iter_id);

    printf("nseq: %ld\n", nseq);
    printf("nelem: %ld\n", nelem);

    printf("sranks: %d, ", sranks);
    printf("seq_start_off: [", seq_start_off);

    for (int i=0; i< sranks; i++){
        printf("%zu, ", seq_start_off[i]);
    }
    printf("], ");
    printf("seq_len: %d, ", seq_len);
    // // printf("seq_start_off: %ld\n", seq_start_off);
    // for (int i = 0; i < ndim; ++i) {
    //     printf("seq_start_off[%d]: %d\n", i, seq_start_off[i]);
    // }
    // printf("seq_len: %ld\n", seq_len);

    // 4. get the selected rows and cols;
    hsize_t start[sranks];
    hsize_t end[sranks];
    herr_t status = H5Sget_select_bounds(space_id, start, end);
    int32_t sub_rows = 1;
    for (int i = 0; i < sranks - 1; ++i) {
        sub_rows *= (end[i] - start[i] + 1);
    }
    // subRegionInfo->sub_rows = sub_rows;
    // subRegionInfo->sub_cols = end[sranks - 1] - start[sranks - 1] + 1;
    // subRegionInfo->g_rows = g_rows;
    // subRegionInfo->g_cols = g_cols;
    sub_cols = end[sranks - 1] - start[sranks - 1] + 1;

    // check file_space selection and mem_space selection
    if(stype == H5S_SEL_NONE){
        printf("H5S_SEL_NONE");
    } else if (stype == H5S_SEL_POINTS){
        printf("H5VL_arrow_get_selected_sub_region ----  type_size: %zu, ", type_size);
        printf("s_row_idx: %d, ", s_row_idx);
        printf("s_col_idx: %d, ", s_col_idx);
        printf("sub_rows: %d, ", sub_rows);
        printf("sub_cols: %d, ", sub_cols);
        printf("g_rows: %ld, ", g_rows);
        printf("g_cols: %ld, ", g_cols);

        printf("H5Sget_select_bounds : {");
        for (int i = 0; i < sranks; ++i) {
            printf("start[%d]: %ld, ", i, start[i]);
            printf("end[%d]: %ld, ", i, end[i]);
        }
        printf("} ");
        printf("H5Sget_select_npoints : %ld ", H5Sget_select_npoints(space_id));
        printf("H5Sget_select_elem_npoints : %ld ", H5Sget_select_elem_npoints(space_id));

    } else if (stype == H5S_SEL_HYPERSLABS){
        printf("H5VL_arrow_get_selected_sub_region ----  type_size: %zu, ", type_size);
        printf("s_row_idx: %d, ", s_row_idx);
        printf("s_col_idx: %d, ", s_col_idx);
        printf("sub_rows: %d, ", sub_rows);
        printf("sub_cols: %d, ", sub_cols);
        printf("g_rows: %ld, ", g_rows);
        printf("g_cols: %ld, ", g_cols);
        printf("H5Sget_select_bounds : {");
        for (int i = 0; i < sranks; ++i) {
            printf("start[%d]: %ld, ", i, start[i]);
            printf("end[%d]: %ld, ", i, end[i]);
        }
        printf("} ");        
        printf("H5Sget_select_hyper_nblocks : %ld ", H5Sget_select_hyper_nblocks(space_id));
    } else if (stype == H5S_SEL_ALL){
        // printf("H5S_SEL_ALL ");
        printf("H5VL_arrow_get_selected_sub_region ----  type_size: %zu, ", type_size);
        printf("s_row_idx: %d, ", s_row_idx);
        printf("s_col_idx: %d, ", s_col_idx);
        printf("sub_rows: %d, ", sub_rows);
        printf("sub_cols: %d, ", sub_cols);
        printf("g_rows: %ld, ", g_rows);
        printf("g_cols: %ld, ", g_cols);

        printf("H5Sget_select_bounds : {");
        for (int i = 0; i < sranks; ++i) {
            printf("start[%d]: %ld, ", i, start[i]);
            printf("end[%d]: %ld, ", i, end[i]);
        }
        printf("} ");
        printf("H5Sget_select_npoints : %ld ", H5Sget_select_npoints(space_id));
        printf("H5Sget_select_elem_npoints : %ld ", H5Sget_select_elem_npoints(space_id));
        printf("H5Sget_select_hyper_nblocks : %ld ", H5Sget_select_hyper_nblocks(space_id));

    } else if (stype == H5S_SEL_N){
        printf("H5S_SEL_N ");
    } else {
        printf("H5S_SEL_ERROR ");
    }
    

    printf("\n");

    // return 0;
}



char* get_datatype_class_str(hid_t type_id) {
    switch (type_id) {
        case H5T_NO_CLASS:
            return "H5T_NO_CLASS";
        case H5T_INTEGER:
            return "H5T_INTEGER";
        case H5T_FLOAT:
            return "H5T_FLOAT";
        case H5T_TIME:
            return "H5T_TIME";
        case H5T_STRING:
            return "H5T_STRING";
        case H5T_BITFIELD:
            return "H5T_BITFIELD";
        case H5T_OPAQUE:
            return "H5T_OPAQUE";
        case H5T_COMPOUND:
            return "H5T_COMPOUND";
        case H5T_REFERENCE:
            return "H5T_REFERENCE";
        case H5T_ENUM:
            return "H5T_ENUM";
        case H5T_VLEN:
            return "H5T_VLEN";
        case H5T_ARRAY:
            return "H5T_ARRAY";
        default:
            return "H5T_NCLASSES";
    }
}

char* get_dataspace_class_str(H5S_class_t class_id) {
    switch (class_id) {
        case H5S_NO_CLASS:
            return "H5S_NO_CLASS";
        case H5S_SCALAR:
            return "H5S_SCALAR";
        case H5S_SIMPLE:
            return "H5S_SIMPLE";
        case H5S_NULL:
            return "H5S_NULL";
        default:
            return "H5S_NO_CLASS";
    }
}

void file_info_update(char * func_name, void * obj, hid_t fapl_id, hid_t fcpl_id, hid_t dxpl_id)
{
    H5VL_tracker_t *file = (H5VL_tracker_t *)obj;
    file_tkr_info_t * file_info = (file_tkr_info_t*)file->generic_tkr_info;

    // get file intent
    if(!file_info->intent){
        char * intent = file_get_intent(file->under_object, file->under_vol_id, dxpl_id);
        file_info->intent = intent ? strdup(intent) : NULL;
    }
    
    hsize_t file_size = file_get_size(file->under_object, file->under_vol_id, dxpl_id);
    file_info->file_size = file_size;

    if(file_info->fapl_id != NULL){
        if(!file_info->header_size){
            H5Pget_meta_block_size(file_info->fapl_id, &file_info->header_size);
        }

        if(!file_info->sieve_buf_size){
            H5Pget_sieve_buf_size(file_info->fapl_id, &file_info->sieve_buf_size);
        }
        if(!file_info->alignment || !file_info->threshold){
            if(H5Pget_alignment(file_info->fapl_id, &file_info->threshold, &file_info->alignment) < 0){
                file_info->alignment = -1;
                file_info->threshold = -1;
            }
        }
    }

#ifdef TRACKER_LOGGING
    file_info_print(func_name, obj, fapl_id, fcpl_id, dxpl_id);
#endif
}

void file_info_print(char * func_name, void * obj, hid_t fapl_id, hid_t fcpl_id, hid_t dxpl_id)
{
    H5VL_tracker_t *file = (H5VL_tracker_t *)obj;
    file_tkr_info_t * file_info = (file_tkr_info_t*)file->generic_tkr_info;

    printf("{\"func_name\": \"%s\", ", func_name);
    printf("\"io_access_idx\": %d, ", -1 );
    printf("\"time(us)\": %ld, ", get_time_usec());
    // printf("\"file_name_addr\": \"%p\", ", file_info->file_name);
    if(fapl_id != NULL){
        printf("\"fapl_id\": %p, ", fapl_id);
        hsize_t curr_offset;
        H5Pget_family_offset(file_info->fapl_id, &curr_offset);
        printf("\"H5Pget_family_offset\": %ld, ", curr_offset);

        int mdc_nelmts;
        size_t rdcc_nslots;
        size_t rdcc_nbytes;
        double rdcc_w0;
        if(H5Pget_cache(fapl_id, &mdc_nelmts, &rdcc_nslots, &rdcc_nbytes, &rdcc_w0) > 0){
            printf("\"H5Pget_cache-mdc_nelmts\": %d, ", mdc_nelmts); // TODO: ?
            printf("\"H5Pget_cache-rdcc_nslots\": %ld, ", rdcc_nslots);
            printf("\"H5Pget_cache-rdcc_nbytes\": %ld, ", rdcc_nbytes);
            printf("\"H5Pget_cache-rdcc_w0\": %f, ", rdcc_w0); // TODO: ?
        }
        size_t buf_size;
        unsigned min_meta_perc;
        unsigned min_raw_perc;
        if(H5Pget_page_buffer_size(fapl_id, &buf_size, &min_meta_perc, &min_raw_perc) > 0){
        printf("\"H5Pget_page_buffer_size-buf_size\": %ld, ", buf_size);
        printf("\"H5Pget_page_buffer_size-min_meta_perc\": %d, ", min_meta_perc); // TODO: ?
        printf("\"H5Pget_page_buffer_size-min_raw_perc\": %ld, ", min_raw_perc);
        }
    }

    // void * buf_ptr_ptr;
    // size_t buf_len_ptr;
    // H5Pget_file_image(file_info->fapl_id, &buf_ptr_ptr, &buf_len_ptr);

    H5F_fspace_strategy_t strategy;
    hbool_t persist;
    hsize_t threshold;
    if(fcpl_id != NULL){
        if(H5Pget_file_space_strategy(fcpl_id, &strategy, &persist, &threshold) > 0){
            printf("\"strategy\": %ld, ", strategy);
            printf("\"persist\": %d, ", persist);
            printf("\"threshold\": %ld, ", threshold);
        }
        hsize_t fsp_size;
        if(H5Pget_file_space_page_size(fcpl_id, &fsp_size) > 0){
            printf("\"fsp_size\": %ld, ", fsp_size);
        }
    }

    // printf("{\"file\": ");
    printf("H5Pget_alignment-threshold: %ld, ", file_info->threshold);
    printf("H5Pget_alignment-alignment: %ld, ", file_info->alignment);
    // printf("\"file_name_hex\": %p, ", file_info->file_name);
    printf("\"file_no\": %d, ", file_info->file_no); //H5FD_t *_file->fileno same
    printf("\"access_size\": %d, ", 0);
    printf("\"offset\": %d, ", -1);
    printf("\"logical_addr\": %d, ", -1);
    printf("\"blob_idx\": %d, ", -1);
    printf("\"header_size\": %ld, ", file_info->header_size);
    printf("\"sieve_buf_size\": %ld, ", file_info->sieve_buf_size);
    printf("\"file_size\": %ld, ", file_info->file_size);
    printf("\"file_intent\": [\"%s\"], ", file_get_intent(file->under_object, file->under_vol_id, dxpl_id));
    printf("\"file_name\": \"%s\", ", file_info->file_name);
    printf("\"file_addr\": %p, ", obj);
    printf("}\n");

}

void attribute_info_print(char * func_name, void *obj,  const H5VL_loc_params_t *loc_params,
    H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req)
{
    printf("{\"func_name\": \"%s\", ", func_name);

    H5VL_tracker_t *file = (H5VL_tracker_t *)obj;
    // file_tkr_info_t * file_info = (file_tkr_info_t*)file->generic_tkr_info;
    // printf("\"file_name\": \"%s\", ", file_info->file_name);

    attribute_tkr_info_t * attr_info = (attribute_tkr_info_t*)file->generic_tkr_info;
    file_tkr_info_t * file_info = attr_info->obj_info.file_info;

    printf("\"attr_token\": %ld, ", attr_info->obj_info.token);

    if(strcmp(func_name,"H5VLattr_open") == 0)
        printf("\"dataset_name\": \"%s\", ", attr_info->obj_info.name);
    else
        printf("\"attr_name\": \"%s\", ", attr_info->obj_info.name);

    printf("\"file_name\": \"%s\", ", file_info->file_name);


    printf("}\n");
}

void group_info_print(char * func_name, void *obj, void *args,
    const char *name, hid_t gapl_id, hid_t dxpl_id, void **req)
{
    // TODO: use if else to get different args
    // if(strcmp(func_name, "H5VLgroup_get") == 0)
    //     H5VL_group_get_args_t * extra_args = (H5VL_group_get_args_t*)args;
    // else 
    //     H5VL_loc_params_t * extra_args = (H5VL_loc_params_t*)args;
    //     // "H5VLgroup_open" "H5VLgroup_create"

    printf("{\"func_name\": \"%s\", ", func_name);

    if(strcmp(func_name,"H5VLgroup_get") == 0){
        // H5VL_group_get_args_t * group_args = (H5VL_group_get_args_t*)args;

        H5VL_tracker_t *group = (H5VL_tracker_t *)obj;
        group_tkr_info_t* group_info = (group_tkr_info_t*)group->generic_tkr_info;

        printf("\"group_token\": %ld, ", group_info->obj_info.token);
        printf("\"group_name\": \"%s\", ", group_info->obj_info.name);

        file_tkr_info_t * file_info = group_info->obj_info.file_info;
        printf("\"file_name\": \"%s\", ", file_info->file_name);

    } else {
        // H5VL_loc_params_t * loc_params = (H5VL_loc_params_t*)args;

        printf("\"group_name\": \"%s\", ", name);

        H5VL_tracker_t *file = (H5VL_tracker_t *)obj;
        file_tkr_info_t * file_info = (file_tkr_info_t*)file->generic_tkr_info;

        group_tkr_info_t* group_info = file_info->opened_grps;
        printf("\"opened_grps_cnt\": %d, ", file_info->opened_grps_cnt);
        if(file_info->opened_grps_cnt > 0){
            // only 1 dset opened at a time
            // dataset_tkr_info_t * dset_info = malloc(sizeof(dataset_tkr_info_t));
            while (group_info) {

                printf("\"token\": %ld, ", group_info->obj_info.token);
                printf("\"name\": \"%s\", ", group_info->obj_info.name);

                group_info = group_info->next; // Move to the next node
            }
        }

        printf("\"file_name\": \"%s\", ", file_info->file_name);

    }

    printf("}\n");
}

char * H5S_select_type_to_str(H5S_sel_type type){
    switch (type)
    {
    case H5S_SEL_NONE:
        return "H5S_SEL_NONE";
    case H5S_SEL_HYPERSLABS:
        return "H5S_SEL_HYPERSLABS";
    case H5S_SEL_ALL:
        return "H5S_SEL_ALL";
    case H5S_SEL_N:
        return "H5S_SEL_N";
    default:
        return "H5S_SEL_ERROR";
    }
}

// TODO: update dataset_info to tracking object
void dataset_info_update(char * func_name, hid_t mem_type_id, hid_t mem_space_id,
    hid_t file_space_id, void * obj, hid_t dxpl_id)
{
    H5VL_tracker_t *dset = (H5VL_tracker_t *)obj;
    dataset_tkr_info_t * dset_info = (dataset_tkr_info_t*)dset->generic_tkr_info; 
    if(!dset_info->dspace_id)
        dset_info->dspace_id = dataset_get_space(dset->under_object, dset->under_vol_id, dxpl_id);

    hid_t space_id = dset_info->dspace_id;
    
    if (!dset_info->dimension_cnt){
        unsigned int dimension_cnt = H5Sget_simple_extent_ndims(space_id);
        dset_info->dimension_cnt = dimension_cnt;
        if(dimension_cnt > 0)
            dset_info->dimensions = (hsize_t *)malloc( dimension_cnt * sizeof(hsize_t *));
        hsize_t maxdims[dimension_cnt];
        H5Sget_simple_extent_dims(space_id, dset_info->dimensions, maxdims);
    }
    // unsigned int ndim = (unsigned)H5Sget_simple_extent_ndims(space_id);
    // hsize_t dimensions[ndim];
    // H5Sget_simple_extent_dims(space_id, dimensions, NULL);

    if(!dset_info->dset_select_type){
        H5S_sel_type sel_stype = H5Sget_select_type(space_id);
        dset_info->dset_select_type = H5S_select_type_to_str(sel_stype);
    }
    if(!dset_info->dset_select_npoints && H5Iis_valid(space_id) > 0){
        dset_info->dset_select_npoints = H5Sget_select_npoints(space_id);
    }

    if(!dset_info->dset_n_elements && dset_info->dimension_cnt > 0){
        size_t nelems = 1;
        for(int i=0; i< dset_info->dimension_cnt; i++){
            nelems = nelems * dset_info->dimensions[i];
        }
        dset_info->dset_n_elements = nelems;
    }

    if(!dset_info->layout)
    {
        // get layout type (only available at creation time?)
        hid_t dcpl_id = dataset_get_dcpl(dset->under_object, dset->under_vol_id,dxpl_id);
        // only valid with creation property list
        char * layout = dataset_get_layout(dcpl_id); 
        // dset_info->layout = layout ? strdup(layout) : NULL;
        dset_info->layout = (char*) malloc(sizeof(char) * (strlen(layout) + 1));
        strcpy(dset_info->layout, layout);
    }


    if(strcmp(func_name,"H5VLdataset_create") != 0 
        && strcmp(func_name,"H5VLget_object") != 0 ){
        if((dset_info->storage_size == NULL) || (dset_info->storage_size < 1))
        {
            // get storage size
            size_t tracker_page_size = TKR_HELPER->tracker_page_size;
            size_t start_addr;
            size_t end_addr;
            size_t start_page;
            size_t end_page;
            size_t access_size;

            tkrLockAcquire(&myLock);
            dset_info->storage_size = dataset_get_storage_size(dset->under_object, dset->under_vol_id, dxpl_id);
            
            start_addr = START_ADDR;
            end_addr = END_ADDR;
            access_size = ACC_SIZE;
            tkrLockRelease(&myLock);

            start_page = start_addr/tracker_page_size;
            end_page = end_addr/tracker_page_size;

            if(access_size == dset_info->storage_size){
                dset_info->data_file_page_start = start_page;
                dset_info->data_file_page_end = end_page;
            } else {
                dset_info->data_file_page_start = end_page;
                dset_info->data_file_page_end = (end_addr + dset_info->storage_size) / tracker_page_size;
            }

            if(strcmp(dset_info->layout,"H5D_CONTIGUOUS") == 0)
                dset_info->storage_size = dset_info->dset_n_elements * dset_info->dset_type_size;

        }

    } //&& strcmp(func_name,"H5VLget_object") != 0

    // if(strcmp(dset_info->layout,"H5D_CONTIGUOUS") == 0)
    if ((dset_info->dset_offset == NULL) || (dset_info->dset_offset < 0) && dxpl_id != NULL)
        dset_info->dset_offset = dataset_get_offset(dset->under_object, dset->under_vol_id, dxpl_id);

#ifdef TRACKER_LOGGING
    dataset_info_print(func_name, mem_type_id, mem_space_id, file_space_id, obj, dxpl_id);
#endif

}

void dataset_info_print(char * func_name, hid_t mem_type_id, hid_t mem_space_id,
    hid_t file_space_id, void * obj, hid_t dxpl_id)
{

    
    H5VL_tracker_t *dset = (H5VL_tracker_t *)obj;
    dataset_tkr_info_t * dset_info = (dataset_tkr_info_t*)dset->generic_tkr_info;    

    // if(mem_type_id != NULL)
    //     H5VL_arrow_get_selected_sub_region(dset_info->dspace_id, H5Tget_size(dset_info->dtype_id));


    // assert(dset_info);

    printf("{\"func_name\": \"%s\", ", func_name);

    printf("\"vol_obj\": %ld, ", obj);
    printf("\"vol_under_obj\": %ld, ", dset->under_object);

    if(!dxpl_id)
        printf("\"dxpl_id_vol\": %d, ", -1);
    else
        printf("\"dxpl_id_vol\": %ld, ", dxpl_id);
    
    printf("\"time(us)\": %ld, ", get_time_usec());


    printf("\"dset_name\": \"%s\", ", dset_info->obj_info.name);

    printf("\"dset_token\": %ld, ", dset_info->obj_info.token);

    // printf("\"dset_token.__data\": [");
    // for (size_t i = 0; i < 16; i++)
    // {
    //     printf("%ld, ", dset_info->obj_info.token.__data[i]);

    // }
    // printf("], ");

    printf("\"layout\": \"%s\", ", dset_info->layout); //TODO
    printf("\"dt_class\": \"%s\", ", get_datatype_class_str(dset_info->dt_class));

    // printf("\"offset\": %ld, ", dataset_get_offset(dset->under_object, dset->under_vol_id, dxpl_id));
    printf("\"offset\": %ld, ", dset_info->dset_offset);

    printf("\"type_size\": %ld, ", dset_info->dset_type_size);
    printf("\"storage_size\": %ld, ", dset_info->storage_size);
    printf("\"n_elements\": %ld, ", dset_info->dset_n_elements);
    printf("\"dimension_cnt\": %d, ", dset_info->dimension_cnt);
    // print dimensions
    printf("\"dimensions\": [");
    if(dset_info->dimension_cnt > 0 && dset_info->dimension_cnt != -1){
        for(int i=0; i<dset_info->dimension_cnt; i++)
            printf("%ld,",dset_info->dimensions[i]);
    }

    printf("], ");

    printf("\"file_name\": \"%s\", ", dset_info->pfile_name);
    // printf("\"blob_idx\": %d, ", -1);
    printf("\"dxpl_id\": %d, ", dxpl_id);

    printf("}\n");
}

uint32_t decode_uint32(uint32_t value, const uint8_t *p) {
    // uint32_t value = 0;
    
    value |= (uint32_t)(*p++) << 24;
    value |= (uint32_t)(*p++) << 16;
    value |= (uint32_t)(*p++) << 8;
    value |= (uint32_t)(*p++);
    
    return value;
}

haddr_t * decode_to_addr(void ** p){

/* Reset value in destination */
    hbool_t  all_zero = true; /* True if address was all zeroes */
    unsigned u;               /* Local index variable */
    haddr_t *addr_p = (haddr_t *) malloc(sizeof(haddr_t));
    uint8_t **pp = (uint8_t **)p;
    size_t addr_len = sizeof(haddr_t);

    /* Decode bytes from address */
    for (u = 0; u < addr_len; u++) {
        uint8_t c; /* Local decoded byte */

        /* Get decoded byte (and advance pointer) */
        c = *(*pp)++;

        /* Check for non-undefined address byte value */
        if (c != 0xff)
            all_zero = false;
        
        // printf("blob_id_debug_to_addr %d \n", u);

        if (u < sizeof(*addr_p)) {
            haddr_t tmp = c; /* Local copy of address, for casting */

            /* Shift decoded byte to correct position */
            tmp <<= (u * 8); /*use tmp to get casting right */

            // printf("blob_id_debug_to_addr %d %d \n",u,u);

            /* Merge into already decoded bytes */
            *addr_p |= tmp;

            // printf("blob_id_debug_to_addr %d %d %d\n",u,u,u);
        } /* end if */
        else if (!all_zero)
            HDassert(0 == **pp); /*overflow */
    }                            /* end for */

    /* If 'all_zero' is still TRUE, the address was entirely composed of '0xff'
     *  bytes, which is the encoded form of 'HADDR_UNDEF', so set the destination
     *  to that value */
    if (all_zero)
        *addr_p = HADDR_UNDEF;

    return addr_p;
}

// void blob_info_print(char * func_name, void * obj, hid_t dxpl_id, 
//     size_t size, void * blob_id, const void * buf,void *ctx)
// {
//     H5VL_tracker_t *dset = (H5VL_tracker_t *)obj;
//     dataset_tkr_info_t * dset_info = (dataset_tkr_info_t*)dset->generic_tkr_info;
//     assert(dset_info);
//     // printf("\"dset_name\": \"%s\", ", dset_info->obj_info.name); //TODO
//     // char *token_str = NULL;
//     // printf("\"dset_token_str\": \"%s\", ", H5Otoken_to_str(space_id, &dset_info->obj_info.token, &token_str));
//     printf("\"dset_token\": %ld, ", dset_info->obj_info.token);
//     printf("\"storage_size\": %ld, ", dset_info->storage_size);
//     printf("\n");
// }

void blob_info_print(char * func_name, void * obj, hid_t dxpl_id, 
    size_t size, void * blob_id, const void * buf,void *ctx)
{
    H5VL_tracker_t *file = (H5VL_tracker_t *)obj;
    file_tkr_info_t * file_info = (file_tkr_info_t*)file->generic_tkr_info;

    printf("{\"func_name\": \"%s\", ", func_name);


    if(!obj)
        obj = (void*) -1;
    if(!size)
        size = (size_t)-1;
    if(!blob_id)
        blob_id = (void*)-1;
    if(!ctx)
        ctx = (void*)-1;

    assert(file_info);

    // H5Pget_elink_file_cache_size(file_info->fapl_id, &efc_size);
    // H5Pget_fapl_direct(file_info->fapl_id, &boundary, &block_size, &cbuf_siz);
    // H5Pget_family_offset(file_info->fapl_id, &curr_offset);
    // void * under_driver = H5Pget_driver_info(file_info->fapl_id);
    // H5Pget_small_data_block_size(file_info->fapl_id, &sm_db_size);

    hsize_t dummy01 = 0;

	int mdc_nelmts;
    size_t rdcc_nslots;
    size_t rdcc_nbytes;
    double rdcc_w0;
    // if(H5Pget_cache(file_info->fapl_id, &mdc_nelmts, &rdcc_nslots, &rdcc_nbytes, &rdcc_w0) > 0){
    //     printf("\"H5Pget_cache-mdc_nelmts\": %d, ", mdc_nelmts); // TODO: ? 0
    //     printf("\"H5Pget_cache-rdcc_nslots\": %ld, ", rdcc_nslots);
    //     printf("\"H5Pget_cache-rdcc_nbytes\": %ld, ", rdcc_nbytes);
    //     printf("\"H5Pget_cache-rdcc_w0\": %f, ", rdcc_w0); // TODO: ? 0.000000
    // }
    H5Pget_cache(file_info->fapl_id, &mdc_nelmts, &rdcc_nslots, &rdcc_nbytes, &rdcc_w0);
    printf("\"H5Pget_cache-rdcc_nslots\": %ld, ", rdcc_nslots);
    printf("\"H5Pget_cache-rdcc_nbytes\": %ld, ", rdcc_nbytes);


    hsize_t threshold;
    hsize_t alignment;
    H5Pget_alignment(file_info->fapl_id, &threshold, &alignment);
    void * buf_ptr_ptr;
    size_t buf_len_ptr;
    H5Pget_file_image(file_info->fapl_id, &buf_ptr_ptr, &buf_len_ptr);

    size_t buf_size;
    unsigned min_meta_perc;
    unsigned min_raw_perc;
    // if(H5Pget_page_buffer_size(file_info->fapl_id, &buf_size, &min_meta_perc, &min_raw_perc) > 0){
    //     printf("\"H5Pget_page_buffer_size-buf_size\": %ld, ", buf_size);
    //     printf("\"H5Pget_page_buffer_size-min_meta_perc\": %d, ", min_meta_perc); // TODO: ?
    //     printf("\"H5Pget_page_buffer_size-min_raw_perc\": %ld, ", min_raw_perc);
    // }
    H5Pget_page_buffer_size(file_info->fapl_id, &buf_size, &min_meta_perc, &min_raw_perc);
    printf("\"H5Pget_page_buffer_size-buf_size\": %ld, ", buf_size);
    

    printf("\"blob_id\": %ld, ", blob_id);

    haddr_t * addr_p = decode_to_addr(&blob_id);
    printf("\"addr_p\": %ld, ", addr_p);
    printf("Content at addr_p : %zu, ", *(unsigned char*) (long long) addr_p);
    
    if(!size)
        printf("\"access_size\": %d, ", 0);
    else
        printf("\"access_size\": %ld, ", size);

    // int ndset = file_info->opened_datasets_cnt;
    printf("\"opened_datasets_cnt\": %d, ", file_info->opened_datasets_cnt);
    size_t num_datasets = sizeof(&file_info->opened_datasets) / sizeof(&file_info->opened_datasets[0]);
    printf("\"num_datasets\": %d, ", num_datasets);
    printf("\"opened_datasets\": \n\t");

    dataset_tkr_info_t* dset_info = file_info->opened_datasets;

    if(file_info->opened_datasets_cnt > 0){
        // only 1 dset opened at a time
        // dataset_tkr_info_t * dset_info = malloc(sizeof(dataset_tkr_info_t));
        while (dset_info) {
            // dset_info = (dataset_tkr_info_t*) &file_info->opened_datasets[ndset];
            assert(dset_info);
            hid_t space_id = dset_info->dspace_id;
            // hid_t type_id = dset_info->dtype_id;
            // printf("\"space_id_addr\": %p, ", space_id);
            
            // hsize_t * dimensions = dset_info->dimensions;
            size_t io_access_idx = dset_info->blob_put_cnt + dset_info->blob_get_cnt 
            + dset_info->dataset_read_cnt + dset_info->dataset_write_cnt -1;

            printf("{\"io_access_idx\": %ld, ", io_access_idx );

            printf("\"time(us)\": %ld, ", get_time_usec());

            printf("\"dset_name\": \"%s\", ", dset_info->obj_info.name); //TODO
        
            // char *token_str = NULL;
            // printf("\"dset_token_str\": \"%s\", ", H5Otoken_to_str(space_id, &dset_info->obj_info.token, &token_str));
            printf("\"dset_token\": %ld, ", dset_info->obj_info.token);

            size_t  token_idx = (size_t) malloc(sizeof(size_t));
            decode_uint32((size_t) &dset_info->obj_info.token, (const uint8_t*)token_idx);
            // printf("\"token_idx\": %zu, ", token_idx);
            long long token_idx_addr = (long long) token_idx;
            unsigned char* tptr = (unsigned char*)token_idx_addr;
            // printf("Content at token_idx : %u, ", *tptr);

            // printf("\"obj_token\": %ld, ", dset_info->obj_info.token); // a fixed number 800
            printf("\"file_no\": %ld, ", file_info->file_no); // matches dset_name
            printf("\"offset\": %ld, ", dset_info->dset_offset); //TODO: get blob offset
            printf("\"layout\": \"%s\", ", dset_info->layout); //TODO: blob layout?
            printf("\"dt_class\": \"%s\", ", get_datatype_class_str(dset_info->dt_class));
            printf("\"storage_size\": %ld, ", dset_info->storage_size);
            printf("\"n_elements\": %ld, ", dset_info->dset_n_elements);
            printf("\"dimension_cnt\": %d, ", dset_info->dimension_cnt); //H5Sget_simple_extent_ndims gets negative

            printf("\"dimensions\": [");
            if(dset_info->dimensions){
                for(int i=0; i<dset_info->dimension_cnt; i++)
                    printf("%ld,", dset_info->dimensions[i]);
            }
            printf("], ");

            printf("\"dset_addr\": %p, ", obj);
            printf("}\n");
            dset_info = dset_info->next; // Move to the next node
        }


    }


    int *dm1, *dm2, *dm3, *dm4, *dm5, *dm6, *dm7, *dm8, *dm9, *dm10, *dm11, *dm12, *dm13, *dm14, *dm15;// *dm16;// *dm17, *dm18;// *dm19;
    dm1 = dm2 = dm3 = dm4 = dm5 = dm6 = dm7 = dm8 = dm9 = dm10 = dm11 = dm12 = dm13 = dm14 = dm15 =0;// dm16 =0;// dm17 =0;// dm18 = 0;

    // if (dummy0 == 0)
    // {
    //     size_t first_put_addr = dset_info->total_bytes_written + dset_info->dset_offset;
    //     // printf("\"logical_addr\": %ld, ", dm9);
    //     // printf("\"dset_info->storage_size\": %ld, ", dset_info->storage_size);    
    //     if(first_put_addr < (dset_info->dset_type_size * dset_info->dset_n_elements))
    //         printf("\"logical_addr\": %ld, ", (&dummy0-1));
    //     else
    //         printf("\"logical_addr\": %ld, ", 0); // first_put_addr incorrect
    // } else
    //     printf("\"logical_addr\": %ld, ", dummy0);

    // printf("\"logical_addr\": %ld, ", dummy0);
    if(!dxpl_id)
        printf("\"dxpl_id_vol\": %d, ", -1);
    else
        printf("\"dxpl_id_vol\": %ld, ", dxpl_id);

    printf("\"file_name\": \"%s\", ", file_info->file_name);
    // printf("\"blob_idx\": %ld, ", * (&dummy0-14)); // either -13/-14
    printf("}\n");
    
    // BLOB_SORDER+=1; // increment blob order
}


    /* candice added routine implementation end*/

char* encode_two_strings(const char* file_path, const char* dset_name) {
    // // Get the actual file name from path
    // char* file_name = strrchr(file_path, '/');

    // Keep complete file path name
    char * file_name = (char*)file_path;

    if(file_name)
        file_name++;
    else
        file_name = (char*)file_path;

    size_t file_name_len = strlen(file_name);
    size_t dset_name_len = strlen(dset_name);
    size_t encoded_len = file_name_len + dset_name_len + 1; // 1 for '@'

    char* encoded_str = (char*)malloc(encoded_len + 1); // +1 for null terminator
    
    snprintf(encoded_str, encoded_len + 1, "%s@%s", file_name, dset_name);

    // // Copy file_name to encoded_str
    // strncpy(encoded_str, file_name, file_name_len);
    // encoded_str[file_name_len] = '\0'; // Null-terminate the copied file_name

    // // Append '@' to encoded_str
    // encoded_str[file_name_len] = '@';

    // // Copy dset_name to encoded_str after the '@'
    // strncpy(encoded_str + file_name_len + 1, dset_name, dset_name_len);
    // encoded_str[encoded_len] = '\0'; // Null-terminate the entire encoded_str

    return encoded_str;
}

void decode_two_strings(const char* encoded_str, char** file_name, char** dset_name) {
    const char* at_sign = strchr(encoded_str, '@');
    if (at_sign) {
        size_t file_name_len = at_sign - encoded_str;
        size_t dset_name_len = strlen(at_sign + 1);

        *file_name = (char*)malloc(file_name_len + 1);
        strncpy(*file_name, encoded_str, file_name_len);
        (*file_name)[file_name_len] = '\0';

        *dset_name = (char*)malloc(dset_name_len + 1);
        strncpy(*dset_name, at_sign + 1, dset_name_len);
        (*dset_name)[dset_name_len] = '\0';
    } else {
        *file_name = NULL;
        *dset_name = NULL;
    }
}

/* Tracker objects implementations */
void file_ds_created(file_tkr_info_t *info)
{
    assert(info);
    if(info)
        info->ds_created++;
}
void file_grp_created(file_tkr_info_t *info)
{
    assert(info);
    if(info)
        info->grp_created++;
}
void file_dtypes_created(file_tkr_info_t *info)
{
    assert(info);
    if(info)
        info->dtypes_created++;
}




//counting how many times datasets are opened in a file.
//Called by a DS
void file_ds_accessed(file_tkr_info_t* info)
{
    assert(info);
    if(info)
        info->ds_accessed++;
}
void file_grp_accessed(file_tkr_info_t *info)
{
    assert(info);
    if(info)
        info->grp_accessed++;
}
void file_dtypes_accessed(file_tkr_info_t *info)
{
    assert(info);
    if(info)
        info->dtypes_accessed++;
}


/* dataset tracking implementations */
// Create a new dset_track_t object
dset_track_t *create_dset_track_info(dataset_tkr_info_t* dset_info) {
    dset_track_t * track_entry = (dset_track_t *)malloc(sizeof(dset_track_t));

    if (track_entry) {
        // Initialize the fields
        memset(track_entry, 0, sizeof(dset_track_t));
        track_entry->start_time = dset_info->start_time;
        track_entry->end_time = get_time_usec();
        track_entry->token_num = token_to_num(dset_info->obj_info.token);
        track_entry->dt_class = dset_info->dt_class;
        track_entry->ds_class = dset_info->ds_class;
        track_entry->layout = strdup(dset_info->layout);
        track_entry->dimension_cnt = dset_info->dimension_cnt;

        track_entry->dimensions = (hsize_t *)malloc(sizeof(hsize_t) * dset_info->dimension_cnt);
        memcpy(track_entry->dimensions, dset_info->dimensions, sizeof(hsize_t) * dset_info->dimension_cnt);

        track_entry->dset_type_size = dset_info->dset_type_size;
        track_entry->dataset_read_cnt = dset_info->dataset_read_cnt;
        track_entry->dataset_write_cnt = dset_info->dataset_write_cnt;
        track_entry->dset_offset = dset_info->dset_offset;
        track_entry->storage_size = dset_info->storage_size;
        track_entry->dset_n_elements = dset_info->dset_n_elements;
        track_entry->hyper_nblocks = dset_info->hyper_nblocks;

        track_entry->pfile_sorder_id = dset_info->pfile_sorder_id;
        track_entry->data_file_page_start = dset_info->data_file_page_start;
        track_entry->data_file_page_end = dset_info->data_file_page_end;

        track_entry->sorder_ids = NULL;
        track_entry->sorder_ids_end = NULL;
        track_entry->metadata_file_pages = NULL;
        track_entry->metadata_file_pages_end = NULL;

        myll_add(&(track_entry->sorder_ids), &(track_entry->sorder_ids_end), dset_info->sorder_id);
        myll_add(&(track_entry->metadata_file_pages), &(track_entry->metadata_file_pages_end), dset_info->metadata_file_page);
        track_entry->dset_select_type = strdup(dset_info->dset_select_type);
        track_entry->dset_select_npoints = dset_info->dset_select_npoints;

        // add task name
        // const char* curr_task = getenv("CURR_TASK");
        // track_entry->task_name = curr_task ? strdup(curr_task) : NULL;
        char *curr_task = NULL;
        if (getCurrentTask(&curr_task)) {
            // printf("Current task is: %s\n", curr_task);

            // Convert curr_task to char* and assign to info->task_name
            pid_t pid = getpid();
            size_t task_name_len = strlen(curr_task) + 1 + 10; // Extra space for appending the process ID
            char *task_name_cstr = (char *)malloc(task_name_len);
            if (task_name_cstr) {
                snprintf(task_name_cstr, task_name_len, "%s-%d", curr_task, (int)pid);

                track_entry->task_name = task_name_cstr ? strdup(task_name_cstr) : NULL;
                // track_entry->task_name = task_name_cstr;
            } else {
                perror("Failed to allocate memory for task name");
                // Handle the error as needed
            }

            free(curr_task);
        } else {
            fprintf(stderr, "Failed to get current task.\n");
            // Handle the error as needed
        }

    }
    return track_entry;
}

// Cleanup the hash table (using uthash)
void cleanup_hash_table() {
    DsetTrackHashEntry *current, *tmp;

    // Iterate over the hash table and delete each entry using uthash macros
    HASH_ITER(hh, lock.hash_table, current, tmp) {
        HASH_DEL(lock.hash_table, current);
        free(current);
    }

    // Set the hash table pointer to NULL
    lock.hash_table = NULL;
}

// Initialize the lock
void init_hasn_lock() {
    pthread_mutex_init(&(lock.mutex), NULL);
    lock.hash_table = NULL;
}

// Destroy the lock and cleanup the hash table
void destroy_hash_lock() {
    // Lock the mutex to ensure exclusive access during destruction
    pthread_mutex_lock(&(lock.mutex));

    // Cleanup the hash table
    cleanup_hash_table();

    // Destroy the mutex
    pthread_mutex_destroy(&(lock.mutex));
}

// Add a dset_track_t object to the hash table
void add_dset_track_info(char * key, dset_track_t *dset_track_info) {
    DsetTrackHashEntry *entry = (DsetTrackHashEntry *)malloc(sizeof(DsetTrackHashEntry));
    if (entry) {
        entry->key = key;
        entry->dset_track_info = dset_track_info;
        entry->logged = 0;

        // Acquire the lock before modifying the hash table
        pthread_mutex_lock(&(lock.mutex));

        // Add the entry to the hash table
        // HASH_ADD(hh, lock.hash_table, key, strlen(key), entry);
        HASH_ADD_STR(lock.hash_table, key, entry);

        // Release the lock
        pthread_mutex_unlock(&(lock.mutex));
    }
}




// Remove a dset_track_t object from the hash table
void remove_dset_track_info(char * key) {
    DsetTrackHashEntry *entry = NULL;

    // Acquire the lock before modifying the hash table
    pthread_mutex_lock(&(lock.mutex));

    // Find the entry in the hash table
    HASH_FIND_STR(lock.hash_table, key, entry);

    if (entry) {
        // Remove the entry from the hash table
        HASH_DEL(lock.hash_table, entry);

        // Free the memory of the dset_track_t object
        free_dset_track_info(entry->dset_track_info);
        free(entry);
    }

    // Release the lock
    pthread_mutex_unlock(&(lock.mutex));
}


void log_dset_ht_yaml(FILE* f) {
    DsetTrackHashEntry* entry = NULL;

    // Acquire the lock before accessing the hash table
    pthread_mutex_lock(&(lock.mutex));

    // Traverse the hash table and print the token number and key of each entry
    for (entry = lock.hash_table; entry != NULL; entry = entry->hh.next) {
        if(entry->logged == 0) {
            char* file_name = NULL;
            char* dset_name = NULL;
            decode_two_strings(entry->key, &file_name, &dset_name);
            dset_track_t* dset_track_info = entry->dset_track_info;

            fprintf(f, "- file-%ld:\n", dset_track_info->pfile_sorder_id);
            fprintf(f, "    file_name: \"/%s\"\n", file_name);
            // if(dset_track_info->task_name != NULL){
            //     fprintf(f, "    task_name: \"%s\"\n", dset_track_info->task_name);
            // }else{
            //     const char* curr_task = getenv("CURR_TASK");
            //     if (curr_task)
            //         fprintf(f, "    task_name: \"%s\"\n", curr_task);
            //     else
            //         fprintf(f, "    task_name: \"\"\n");
            // }

            // fprintf(f, "    task_name: \"%s\"\n", dset_track_info->task_name);

            if (dset_track_info->task_name != NULL) {
                fprintf(f, "    task_name: \"%s\"\n", dset_track_info->task_name);
            } else {
                char task_name[32]; // Adjust the buffer size as needed
                snprintf(task_name, sizeof(task_name), "%d", getpid());
                fprintf(f, "    task_name: \"%s\"\n", task_name);
            }

            fprintf(f, "    datasets:\n");
            // fprintf(f, "    - dset:\n");
            fprintf(f, "        dset_name: \"%s\"\n", dset_name);
            fprintf(f, "        start_time: %ld\n", dset_track_info->start_time);
            fprintf(f, "        end_time: %ld\n", dset_track_info->end_time);
            // fprintf(f, "      token: %ld\n", dset_track_info->token_num);
            fprintf(f, "        dt_class: \"%s\"\n", get_datatype_class_str(dset_track_info->dt_class));
            fprintf(f, "        ds_class: \"%s\"\n", get_dataspace_class_str(dset_track_info->ds_class));
            fprintf(f, "        layout: \"%s\"\n", dset_track_info->layout);
            fprintf(f, "        storage_size: %ld\n", dset_track_info->storage_size);
            fprintf(f, "        dset_n_elements: %ld\n", dset_track_info->dset_n_elements);
            fprintf(f, "        dimension_cnt: %d\n", dset_track_info->dimension_cnt);
            fprintf(f, "        dimensions: [");
            for (int i = 0; i < dset_track_info->dimension_cnt; i++) {
                fprintf(f, "%ld, ", dset_track_info->dimensions[i]);
            }
            fprintf(f, "]\n");
            fprintf(f, "        dset_type_size: %d\n", dset_track_info->dset_type_size);
            fprintf(f, "        dataset_read_cnt: %d\n", dset_track_info->dataset_read_cnt);
            fprintf(f, "        total_bytes_read: %d\n", (dset_track_info->dataset_read_cnt * dset_track_info->storage_size));
            fprintf(f, "        dataset_write_cnt: %d\n", dset_track_info->dataset_write_cnt);
            fprintf(f, "        total_bytes_written: %d\n", (dset_track_info->dataset_write_cnt * dset_track_info->storage_size));
            if(dset_track_info->dataset_read_cnt > 0 && dset_track_info->dataset_write_cnt == 0) {
                fprintf(f, "        access_type: read_only\n");
            }
            else if (dset_track_info->dataset_read_cnt == 0 && dset_track_info->dataset_write_cnt > 0) {
                fprintf(f, "        access_type: write_only\n");
            } else {
                fprintf(f, "        access_type: read_write\n");
            }
            fprintf(f, "        dset_offset: %ld\n", dset_track_info->dset_offset);
            fprintf(f, "        dset_select_type: \"%s\"\n", dset_track_info->dset_select_type);
            fprintf(f, "        dset_select_npoints: %ld\n", dset_track_info->dset_select_npoints);
            fprintf(f, "        data_file_pages: [%ld,%ld]\n", dset_track_info->data_file_page_start, dset_track_info->data_file_page_end);

            fprintf(f, "        metadata_file_pages: [");
            myll_to_file(f, dset_track_info->metadata_file_pages);
            fprintf(f, "]\n");

            fprintf(f, "        access_orders: [");
            myll_to_file(f, dset_track_info->sorder_ids);
            fprintf(f, "]\n");

            // // Remove the entry from the hash table
            // HASH_DEL(lock.hash_table, entry);

            // // Free the memory of the dset_track_t object
            // free_dset_track_info(entry->dset_track_info);
            // free(entry);

            // // Free the memory of the file_name and dset_name
            // free(file_name);
            // free(dset_name);

            entry->logged = 1;
        }

    }

    // fprintf(f, "\n");

    // Release the lock
    pthread_mutex_unlock(&(lock.mutex));
    fflush(f);

}



// Print info of all dset_track_t objects in the hash table
void print_ht_info() {
    DsetTrackHashEntry *entry = NULL;
    int count = 0;
    printf("Hash table info:\n");

    // Acquire the lock before accessing the hash table
    pthread_mutex_lock(&(lock.mutex));

    // Traverse the hash table and print the token number and key of each entry
    for (entry = lock.hash_table; entry != NULL; entry = entry->hh.next) {
        char *file_name = NULL;
        char *dset_name = NULL;
        decode_two_strings(entry->key, &file_name, &dset_name);
        dset_track_t *dset_track_info = entry->dset_track_info;


        printf("- file-%ld:\n", dset_track_info->pfile_sorder_id);
        printf("  file_name: \"%s\"\n", file_name);
        printf("  dset:\n");
        printf("    dset_name: \"%s\"\n", dset_name);
        printf("    start_time: %ld\n", dset_track_info->start_time);
        printf("    token: %ld\n", dset_track_info->token_num);
        printf("    dt_class: %d\n", dset_track_info->dt_class);
        printf("    ds_class: %d\n", dset_track_info->ds_class);
        printf("    layout: \"%s\"\n", dset_track_info->layout);
        printf("    storage_size: %ld\n", dset_track_info->storage_size);
        printf("    dset_n_elements: %ld\n", dset_track_info->dset_n_elements);
        printf("    dimension_cnt: %d\n", dset_track_info->dimension_cnt);
        printf("    dimensions: [");
        for (int i = 0; i < dset_track_info->dimension_cnt; i++) {
            printf("%ld ", dset_track_info->dimensions[i]);
        }
        printf("]\n");
        printf("    dset_type_size: %d\n", dset_track_info->dset_type_size);
        printf("    dataset_read_cnt: %d\n", dset_track_info->dataset_read_cnt);
        printf("    dataset_write_cnt: %d\n", dset_track_info->dataset_write_cnt);
        printf("    dset_offset: %ld\n", dset_track_info->dset_offset);
        printf("    dset_select_type: \"%s\"\n", dset_track_info->dset_select_type);
        printf("    dset_select_npoints: %ld\n", dset_track_info->dset_select_npoints);
        printf("    data_file_pages: [%ld,%ld]\n", dset_track_info->data_file_page_start, dset_track_info->data_file_page_end);

        // printf("    metadata_file_pages: [");
        // for (int i = 0; i < dset_track_info->metadata_file_page_cnt; i++) {
        //     printf("%ld ", dset_track_info->metadata_file_pages[i]);
        // }
        // printf("]\n");
        count++;
    }
    printf("Total number of tracker entries: %d\n", count);
    printf("\n");

    // Release the lock
    pthread_mutex_unlock(&(lock.mutex));
}



// Print the token numbers of all dset_track_t objects in the hash table
void print_ht_token_numbers() {
    DsetTrackHashEntry *entry = NULL;
    int count = 0;

    // Acquire the lock before accessing the hash table
    pthread_mutex_lock(&(lock.mutex));

    // Traverse the hash table and print the token number and key of each entry
    for (entry = lock.hash_table; entry != NULL; entry = entry->hh.next) {
        printf("Key: %s\n", entry->key);
        printf("Token Number: %ld\n", entry->dset_track_info->token_num);
        count++;
    }
    printf("Total number of tracker entries: %d\n", count);
    printf("\n");

    // Release the lock
    pthread_mutex_unlock(&(lock.mutex));
}

// Free the memory of a dset_track_t object
void free_dset_track_info(dset_track_t *dset_track_info) {
    if (dset_track_info) {
        // free(dset_track_info->file_name);
        // free(dset_track_info->dset_name);

        // free(dset_track_info->layout);
        // free(dset_track_info->dimensions);
        // free(dset_track_info->sorder_ids);
        // free(dset_track_info->sorder_ids_end);
        // free(dset_track_info->metadata_file_pages);
        // free(dset_track_info->metadata_file_pages_end);
        // free(dset_track_info->dset_select_type);
        free(dset_track_info);
    }
}
void myll_free(myll_t **head) {
    myll_t *current = *head;
    while (current != NULL) {
        myll_t *next = current->next;
        free(current);
        current = next;
    }
    *head = NULL;
}



// return the size of the linked list
int myll_print(myll_t *head) {
    printf("Linked List Values: ");
    myll_t *current = head;
    int count =0;
    while (current != NULL) {
        printf("%ld, ", current->data);
        current = current->next;
        count ++;
    }
    printf("\n");
    return count;
}

void myll_to_file(FILE * f, myll_t *head) {
    myll_t *current = head;
    int count =0;
    while (current != NULL) {
        fprintf(f, "%ld, ", current->data);
        current = current->next;
        count ++;
    }
    return count;
}



void myll_add(myll_t **head, myll_t **tail, unsigned long new_data) {
    // Check if the new data already exists in the linked list
    myll_t *current = *head;
    while (current != NULL) {
        if (current->data == new_data){
            // printf("Data [%ld] already exists in the linked list\n", new_data);
            return;
        }
        current = current->next;
    }
    // printf("Adding data [%ld] to the linked list\n", new_data);

    myll_t *new_node = (myll_t *)malloc(sizeof(myll_t));
    if (new_node == NULL) {
        // Handle memory allocation failure
        fprintf(stderr, "Failed to allocate memory for new node\n");
        return;
    }

    new_node->data = new_data;
    new_node->next = NULL;

    if (*head == NULL) {
        *head = new_node;
        *tail = new_node;
    } else {
        (*tail)->next = new_node;
        *tail = new_node;
    }
}



void update_dset_track_info(char * key, dataset_tkr_info_t* dset_info) {
    DsetTrackHashEntry *entry = NULL;

    // Acquire the lock before accessing the hash table
    pthread_mutex_lock(&(lock.mutex));

    // Find the entry in the hash table
    HASH_FIND_STR(lock.hash_table, key, entry);

    if (entry) {
        // Update the token number
        // entry->dset_track_info->token_num = new_token_num;
        size_t new_token = token_to_num(dset_info->obj_info.token);
        if(new_token != entry->dset_track_info->token_num){
            entry->dset_track_info->token_num = new_token;
            // TODO update metadata_file_pages if new_token
        }
        // update end time
        entry->dset_track_info->end_time = get_time_usec();

        if(dset_info->dataset_read_cnt > 0){
            entry->dset_track_info->dataset_read_cnt += dset_info->dataset_read_cnt;
        }
        if(dset_info->dataset_write_cnt > 0){
            entry->dset_track_info->dataset_write_cnt += dset_info->dataset_write_cnt;
        }
        if(dset_info->dset_offset > -1){
            entry->dset_track_info->dset_offset = dset_info->dset_offset;
        }

        myll_add(&(entry->dset_track_info->sorder_ids), &(entry->dset_track_info->sorder_ids_end), dset_info->sorder_id);
        myll_add(&entry->dset_track_info->metadata_file_pages, &entry->dset_track_info->metadata_file_pages_end, dset_info->metadata_file_page);

    }

    // Release the lock
    pthread_mutex_unlock(&(lock.mutex));
}



void add_to_dset_ht(dataset_tkr_info_t* dset_info){

    if(dset_info->pfile_name == NULL || dset_info->obj_info.name == NULL){
        return;
    }
    // Create a DsetTrackKey for the key
    char * key;
    // tkrLockAcquire(&myLock);
    // get encode string name:
    key = encode_two_strings(dset_info->pfile_name, dset_info->obj_info.name);
    // tkrLockRelease(&myLock);

    // Acquire the lock before accessing the hash table
    pthread_mutex_lock(&(lock.mutex));

    // Search for an existing entry with the same key
    DsetTrackHashEntry *existing_entry = NULL;
    HASH_FIND_STR(lock.hash_table, key, existing_entry);

    // Release the lock
    pthread_mutex_unlock(&(lock.mutex));

    // If an existing entry is found, update the value
    if (existing_entry != NULL) {
        // printf("Found existing entry\n");
        update_dset_track_info(key, dset_info);
    } else {
        // printf("No existing entry found\n");
        // Create a dset_track_t object
        dset_track_t *dset_track_info = create_dset_track_info(dset_info);
        // Add the dset_track_info to the hash table
        add_dset_track_info(key, dset_track_info);
    }

    // // Print all the token numbers in the hash table
    // print_ht_token_numbers();

    // // Remove the dset_track_info from the hash table
    // remove_dset_track_info(key);

    // // Free the memory
    // free_dset_track_info(dset_track_info);

}