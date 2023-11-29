/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://www.hdfgroup.org/licenses.               *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Programmer:  Meng Tang
 *              Sep 2023
 *
 * Purpose: The tracker_vfd file driver using only the HDF5 public API
 *          and does POSIX I/O and trackes the file I/O.
 */
#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <assert.h>
// #include <math.h>
// #include <errno.h>


#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
// #include <mutex> // Include the mutex header


/* HDF5 header for dynamic plugin loading */
#include "H5PLextern.h"
#include "H5FD_tracker_vfd.h"     /* Tracker VFD file driver     */
#include "H5FD_tracker_vfd_err.h" /* error handling         */

/* For TRACKER start */
#include <H5FDdevelop.h> /* File driver development macros */
#include "H5FD_tracker_vfd_log.h"

/* private method copies */
#include "H5private.h"
// #include "H5Pprivate.h"
// #include "H5Iprivate.h"

// extern "C" {
// #include "H5FDtracker_vfd_log.h" /* Connecting to vol         */
// }

/* For TRACKER end */


#define H5FD_TRACKER_VFD (H5FD_tracker_vfd_init())
/* HDF5 doesn't currently have a driver init callback. Use
 * macro to initialize driver if loaded as a plugin.
 */
#define H5FD_TRACKER_VFD_INIT                   \
  do {                                          \
    if (H5FD_TRACKER_VFD_g < 0)                 \
      H5FD_TRACKER_VFD_g = H5FD_TRACKER_VFD;    \
  } while (0)

/* File operations */
#define OP_UNKNOWN 0
#define OP_READ    1
#define OP_WRITE   2


/* POSIX I/O mode used as the third parameter to open/_open
 * when creating a new file (O_CREAT is set). */
#if defined(H5_HAVE_WIN32_API)
#define H5FD_TRACKER_VFD_POSIX_CREATE_MODE_RW (_S_IREAD | _S_IWRITE)
#else
#define H5FD_TRACKER_VFD_POSIX_CREATE_MODE_RW 0666
#endif

#define MAXADDR          (((haddr_t)1 << (8 * sizeof(off_t) - 1)) - 1)
#define SUCCEED 0
#define FAIL    (-1)
#define ADDR_OVERFLOW(A) (HADDR_UNDEF == (A) || ((A) & ~(haddr_t)MAXADDR))
#define SIZE_OVERFLOW(Z) ((Z) & ~(hsize_t)MAXADDR)
#define REGION_OVERFLOW(A, Z)                                                                                \
    (ADDR_OVERFLOW(A) || SIZE_OVERFLOW(Z) || HADDR_UNDEF == (A) + (Z) || (off_t)((A) + (Z)) < (off_t)(A))


/* The driver identification number, initialized at runtime */
static hid_t H5FD_TRACKER_VFD_g = H5I_INVALID_HID;

/* Whether to ignore file locks when disabled (env var value) */
static htri_t ignore_disabled_file_locks_s = FAIL;
#define H5F_ACS_FAMILY_TO_SINGLE_NAME "family_to_single"
#define H5F_ACS_IGNORE_DISABLED_FILE_LOCKS_NAME "ignore_disabled_file_locks"

/* Identifiers for HDF5's error API */
hid_t H5FDtracker_vfd_err_stack_g = H5I_INVALID_HID;
hid_t H5FDtracker_vfd_err_class_g = H5I_INVALID_HID;



// #ifdef __cplusplus
// extern "C" {
// #endif




/* Driver-specific file access properties */
typedef struct H5FD_tracker_vfd_fapl_t {
  hbool_t logStat;    /* write to file name on flush */
  size_t  page_size;  /* page size */
  
} H5FD_tracker_vfd_fapl_t;

/* Prototypes */
static herr_t H5FD__tracker_vfd_term(void);
static herr_t  H5FD__tracker_vfd_fapl_free(void *_fa);
static H5FD_t *H5FD__tracker_vfd_open(const char *name, unsigned flags,
                                 hid_t fapl_id, haddr_t maxaddr);
static herr_t H5FD__tracker_vfd_close(H5FD_t *_file);
static int H5FD__tracker_vfd_cmp(const H5FD_t *_f1, const H5FD_t *_f2);
static herr_t H5FD__tracker_vfd_query(const H5FD_t *_f1, unsigned long *flags);
static haddr_t H5FD__tracker_vfd_get_eoa(const H5FD_t *_file, H5FD_mem_t type);
static herr_t H5FD__tracker_vfd_set_eoa(H5FD_t *_file, H5FD_mem_t type,
                                   haddr_t addr);
static haddr_t H5FD__tracker_vfd_get_eof(const H5FD_t *_file, H5FD_mem_t type);
static herr_t H5FD__tracker_vfd_get_handle(H5FD_t *_file, hid_t H5_ATTR_UNUSED fapl, 
                                void **file_handle);
static herr_t H5FD__tracker_vfd_read(H5FD_t *_file, H5FD_mem_t type, hid_t fapl_id,
                                haddr_t addr, size_t size, void *buf);
static herr_t H5FD__tracker_vfd_write(H5FD_t *_file, H5FD_mem_t type, hid_t fapl_id,
                                 haddr_t addr, size_t size, const void *buf);
static herr_t H5FD__tracker_vfd_truncate(H5FD_t *_file, hid_t H5_ATTR_UNUSED dxpl_id, 
                                bool H5_ATTR_UNUSED closing);
static herr_t H5FD__tracker_vfd_lock(H5FD_t *_file, bool rw);
static herr_t H5FD__tracker_vfd_unlock(H5FD_t *_file);
static herr_t H5FD__tracker_vfd_delete(const char *filename, hid_t H5_ATTR_UNUSED fapl_id);
static herr_t H5FD__tracker_vfd_ctl(H5FD_t H5_ATTR_UNUSED *_file, uint64_t H5_ATTR_UNUSED op_code, uint64_t flags,
               const void H5_ATTR_UNUSED *input, void H5_ATTR_UNUSED **output);



static const H5FD_class_t H5FD_tracker_vfd_g = {
  H5FD_CLASS_VERSION,        /* struct version       */
  H5FD_TRACKER_VFD_VALUE,    /* value                */
  H5FD_TRACKER_VFD_NAME,     /* name                 */
  MAXADDR,                   /* maxaddr              */
  H5F_CLOSE_STRONG,          /* fc_degree            */
  H5FD__tracker_vfd_term,    /* terminate            */
  NULL,                      /* sb_size              */
  NULL,                      /* sb_encode            */
  NULL,                      /* sb_decode            */
  sizeof(H5FD_tracker_vfd_fapl_t),/* fapl_size            */
  NULL,                      /* fapl_get             */
  NULL,                      /* fapl_copy            */
  H5FD__tracker_vfd_fapl_free,    /* fapl_free            */
  0,                         /* dxpl_size            */
  NULL,                      /* dxpl_copy            */
  NULL,                      /* dxpl_free            */
  H5FD__tracker_vfd_open,         /* open                 */
  H5FD__tracker_vfd_close,        /* close                */
  H5FD__tracker_vfd_cmp,          /* cmp                  */
  H5FD__tracker_vfd_query,        /* query                */
  NULL,                      /* get_type_map         */
  NULL,                      /* alloc                */
  NULL,                      /* free                 */
  H5FD__tracker_vfd_get_eoa,      /* get_eoa              */
  H5FD__tracker_vfd_set_eoa,      /* set_eoa              */
  H5FD__tracker_vfd_get_eof,      /* get_eof              */
  H5FD__tracker_vfd_get_handle,                      /* get_handle           */
  H5FD__tracker_vfd_read,         /* read                 */
  H5FD__tracker_vfd_write,        /* write                */
  NULL,                      /* read_vector          */
  NULL,                      /* write_vector         */
  NULL,                      /* read_selection       */
  NULL,                      /* write_selection      */
  NULL,                      /* flush                */
  NULL,   /* truncate             */
  NULL,       /* lock                 */
  NULL,     /* unlock               */
  H5FD__tracker_vfd_delete,     /* del                  */
  H5FD__tracker_vfd_ctl,        /* ctl                  */
  H5FD_FLMAP_DICHOTOMY       /* fl_map               */
};

  // H5FD__tracker_vfd_truncate,   /* truncate             */
  // H5FD__tracker_vfd_lock,       /* lock                 */
  // H5FD__tracker_vfd_unlock,     /* unlock               */
  // H5FD__tracker_vfd_delete,     /* del                  */
  // H5FD__tracker_vfd_ctl,        /* ctl                  */

/*-------------------------------------------------------------------------
 * Function:    H5FD_tracker_vfd_init
 *
 * Purpose:     Initialize this driver by registering the driver with the
 *              library.
 *
 * Return:      Success:    The driver ID for the tracker_vfd driver
 *              Failure:    H5I_INVALID_HID
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5FD_tracker_vfd_init(void) {
#ifdef DEBUG_LOG
  std::cout << "H5FD_tracker_vfd_init()" << std::endl;
#endif

  hid_t ret_value = H5I_INVALID_HID; /* Return value */

  /* Initialize error reporting */
  if ((H5FDtracker_vfd_err_stack_g = H5Ecreate_stack()) < 0)
    H5FD_TRACKER_VFD_GOTO_ERROR(H5E_VFL, H5E_CANTINIT, H5I_INVALID_HID,
                           "can't create HDF5 error stack");
  if ((H5FDtracker_vfd_err_class_g = H5Eregister_class(H5FD_TRACKER_VFD_ERR_CLS_NAME,
                                                  H5FD_TRACKER_VFD_ERR_LIB_NAME,
                                                  H5FD_TRACKER_VFD_ERR_VER)) < 0)
    H5FD_TRACKER_VFD_GOTO_ERROR(H5E_VFL, H5E_CANTINIT, H5I_INVALID_HID,
                           "can't register error class with HDF5 error API");

  if (H5I_VFL != H5Iget_type(H5FD_TRACKER_VFD_g))
    H5FD_TRACKER_VFD_g = H5FDregister(&H5FD_tracker_vfd_g);


  /* Set return value */
  ret_value = H5FD_TRACKER_VFD_g;
  // return ret_value;
done:
  H5FD_TRACKER_VFD_FUNC_LEAVE;
} /* end H5FD_tracker_vfd_init() */

/*---------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_term
 *
 * Purpose:     Shut down the VFD
 *
 * Returns:     SUCCEED (Can't fail)
 *
 *---------------------------------------------------------------------------
 */
static herr_t
H5FD__tracker_vfd_term(void) {
  herr_t ret_value = SUCCEED;

  // if(TKR_HELPER_VFD != nullptr)
  //   vfd_tkr_helper_teardown(TKR_HELPER_VFD);

  /* Unregister from HDF5 error API */
  if (H5FDtracker_vfd_err_class_g >= 0) {
    if (H5Eunregister_class(H5FDtracker_vfd_err_class_g) < 0)
      H5FD_TRACKER_VFD_GOTO_ERROR(
        H5E_VFL, H5E_CLOSEERROR, FAIL,
        "can't unregister error class from HDF5 error API");

    // /* Print the current error stack before destroying it */
    PRINT_ERROR_STACK;

    /* Destroy the error stack */
    if (H5Eclose_stack(H5FDtracker_vfd_err_stack_g) < 0) {
      H5FD_TRACKER_VFD_GOTO_ERROR(H5E_VFL, H5E_CLOSEERROR, FAIL,
                             "can't close HDF5 error stack");
      PRINT_ERROR_STACK;
    } /* end if */

    H5FDtracker_vfd_err_stack_g = H5I_INVALID_HID;
    H5FDtracker_vfd_err_class_g = H5I_INVALID_HID;
  }
  /* Reset VFL ID */
  H5FD_TRACKER_VFD_g = H5I_INVALID_HID;


done:
  H5FD_TRACKER_VFD_FUNC_LEAVE_API;
  // return ret_value;
} /* end H5FD__tracker_vfd_term() */

/*-------------------------------------------------------------------------
 * Function:    H5Pset_fapl_tracker_vfd
 *
 * Purpose:     Modify the file access property list to use the H5FD_TRACKER_VFD
 *              driver defined in this source file.  There are no driver
 *              specific properties.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Pset_fapl_tracker_vfd(hid_t fapl_id, hbool_t logStat, size_t page_size, std::string logFilePath) {
  H5FD_tracker_vfd_fapl_t fa; /* Tracker VFD info */
  herr_t ret_value = SUCCEED; /* Return value */

  /* Check argument */
  if (H5I_GENPROP_LST != H5Iget_type(fapl_id) ||
      TRUE != H5Pisa_class(fapl_id, H5P_FILE_ACCESS)) {
    H5FD_TRACKER_VFD_GOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL,
                           "not a file access property list");
  }

  /* Set VFD info values */
  memset(&fa, 0, sizeof(H5FD_tracker_vfd_fapl_t));
  fa.logStat  = logStat;
  fa.page_size = page_size;

  /* Set the property values & the driver for the FAPL */
  if (H5Pset_driver(fapl_id, H5FD_TRACKER_VFD, &fa) < 0) {
    H5FD_TRACKER_VFD_GOTO_ERROR(H5E_PLIST, H5E_CANTSET, FAIL,
                           "can't set Tracker VFD as driver");
  }

  /* custom VFD code start */
  print_H5Pset_fapl_info("H5Pset_fapl_tracker_vfd", logStat, page_size);
  /* custom VFD code end */

done:
  H5FD_TRACKER_VFD_FUNC_LEAVE_API;
}  /* end H5Pset_fapl_tracker_vfd() */


/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_fapl_free
 *
 * Purpose:    Frees the family-specific file access properties.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t H5FD__tracker_vfd_fapl_free(void *_fa) {
  H5FD_tracker_vfd_fapl_t *fa = (H5FD_tracker_vfd_fapl_t *)_fa;
  herr_t ret_value = SUCCEED;  /* Return value */

  free(fa);

  H5FD_TRACKER_VFD_FUNC_LEAVE;
}


/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_open
 *
 * Purpose:     Create and/or opens a file as an HDF5 file.
 *
 * Return:      Success:    A pointer to a new bucket data structure.
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static H5FD_t *
H5FD__tracker_vfd_open(const char *name, unsigned flags, hid_t fapl_id,
                  haddr_t maxaddr) {

#ifdef DEBUG_LOG
  std::cout << "H5FD__open() name = " << name << std::endl;
    /* If an existing file is opened, load the whole file into memory. */
#endif


  H5FD_tracker_vfd_t  *file = NULL; /* tracker_vfd VFD info          */
  int fd = -1;
  int o_flags = 0;
  struct stat     sb = {0};
  int myerrno;
  // hid_t ret_value = H5I_INVALID_HID; /* Return value */
  // H5P_genplist_t *plist;            /* Property list pointer */
  H5FD_t *ret_value = NULL; /* Return value */
  
  /* custom VFD code start */
  unsigned long t_start = get_time_usec();
  unsigned long t1, t2, t_end;
  const H5FD_tracker_vfd_fapl_t *fa   = NULL;
  H5FD_tracker_vfd_fapl_t new_fa = {0};
  ssize_t config_str_len = 0;
  char config_str_buf[MAX_CONF_STR_LENGTH];
  char *saveptr = NULL;
  char* token = NULL;
  char file_path[H5FD_MAX_FILENAME_LEN];

  /* Sanity check on file offsets */
  assert(sizeof(off_t) >= sizeof(size_t));

  H5FD_TRACKER_VFD_INIT;

#ifdef DEBUG_LOG
  std::cout << "H5FD__open() name = " << name << std::endl;
#endif

  /* custom VFD code end */

  /* Check arguments */
  if (!name || !*name)
    H5FD_TRACKER_VFD_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid file name");
  if (0 == maxaddr || HADDR_UNDEF == maxaddr)
    H5FD_TRACKER_VFD_GOTO_ERROR(H5E_ARGS, H5E_BADRANGE, NULL, "bogus maxaddr");
  if (ADDR_OVERFLOW(maxaddr))
    H5FD_TRACKER_VFD_GOTO_ERROR(H5E_ARGS, H5E_OVERFLOW, NULL, "bogus maxaddr");

  /* Build the open flags */
  o_flags = (flags & H5F_ACC_RDWR) ? O_RDWR : O_RDONLY;
  if (flags & H5F_ACC_TRUNC) {
      o_flags |= O_TRUNC;
  }
  if (flags & H5F_ACC_CREAT) {
      o_flags |= O_CREAT;
  }
  if (flags & H5F_ACC_EXCL) {
      o_flags |= O_EXCL;
  }

#ifdef DEBUG_LOG
  std::cout << "H5FD__open() flags = " << flags << std::endl;
#endif

  t1 = get_time_usec();
  /* Open the file */
  if ((fd = HDopen(name, o_flags, H5_POSIX_CREATE_MODE_RW)) < 0) {
      int myerrno = errno;
      H5FD_TRACKER_VFD_GOTO_ERROR(
          H5E_FILE, H5E_CANTOPENFILE, NULL,
          "unable to open file: name = '%s', errno = %d, error message = '%s', flags = %x, o_flags = %x",
          name, myerrno, strerror(myerrno), flags, (unsigned)o_flags);
  } /* end if */

  t2 = get_time_usec();
  TOTAL_POSIX_IO_TIME += (t2 - t1);

#ifdef DEBUG_LOG
  std::cout << "H5FD__open() fd = " << fd << std::endl;
#endif

  // if (HDfstat(fd, &sb) < 0)
  if (fstat(fd, &sb) < 0)
    H5FD_TRACKER_VFD_SYS_GOTO_ERROR(H5E_FILE, H5E_BADFILE, NULL, "unable to fstat file");
      

#ifdef DEBUG_LOG
  std::cout << "H5FD__open() sb.st_size = " << sb.st_size << std::endl;
#endif

  /* Get the driver specific information */
  H5E_BEGIN_TRY {
    fa = static_cast<const H5FD_tracker_vfd_fapl_t*>(H5Pget_driver_info(fapl_id));
  }
  H5E_END_TRY;

  if (!fa || (H5P_FILE_ACCESS_DEFAULT == fapl_id)) {
    if ((config_str_len =
         H5Pget_driver_config_str(fapl_id, config_str_buf, 128)) < 0) {
          printf("H5Pget_driver_config_str error\n");
    }
    *saveptr;
    token = strtok_r(config_str_buf, " ", &saveptr);
    if (!strcmp(token, "true") || !strcmp(token, "TRUE") ||
        !strcmp(token, "True")) {
      new_fa.logStat = true;
    }
    token = strtok_r(0, " ", &saveptr);
    sscanf(token, "%zu", &(new_fa.page_size));
    fa = &new_fa;
  }

#ifdef DEBUG_LOG
  std::cout << "H5FD__open() fa->logStat = " << fa->logStat << std::endl;
  std::cout << "H5FD__open() fa->page_size = " << fa->page_size << std::endl;
#endif

  /* Create the new file struct */
  if (NULL == (file = (H5FD_tracker_vfd_t *)calloc((size_t)1, sizeof(H5FD_tracker_vfd_t))))
      H5FD_TRACKER_VFD_GOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "unable to allocate file struct");

  file->fd = fd;
  file->pos = HADDR_UNDEF;
  file->op  = OP_UNKNOWN;
  /* FIXME: Possible overflow! */
  file->eof = (haddr_t)sb.st_size;
  // H5_CHECKED_ASSIGN(file->eof, haddr_t, sb.st_size, h5_stat_size_t);


    /* Check the file locking flags in the fapl */
    if (ignore_disabled_file_locks_s != FAIL)
        /* The environment variable was set, so use that preferentially */
        file->ignore_disabled_file_locks = ignore_disabled_file_locks_s;


    /* Retain a copy of the name used to open the file, for possible error reporting */
    strncpy(file->filename, name, sizeof(file->filename) - 1);
    file->filename[sizeof(file->filename) - 1] = '\0';


  /* custom VFD code start */
  t_end = get_time_usec();
  
  file->page_size = fa->page_size;
  file->my_fapl_id = fapl_id;
  file->logStat = fa->logStat;


  if(TKR_HELPER_VFD == nullptr){
    
    parseEnvironmentVariable(file_path);
    TKR_HELPER_VFD = vfd_tkr_helper_init(file_path, file->logStat, file->page_size);
  }
  // file->vfd_file_info = add_vfd_file_node(name, file);
  file->vfd_file_info = add_vfd_file_node(TKR_HELPER_VFD, name, file);
  open_close_info_update("H5FD__tracker_vfd_open", file, file->eof, flags, t_start, t_end);
  /* custom VFD code end */

  /* Set return value */
  ret_value = (H5FD_t *)file;


done:
  if (NULL == ret_value) {
    if (fd >= 0)
      close(fd);
    if (file) {
      free(file);
    }
  } /* end if */

  H5FD_TRACKER_VFD_FUNC_LEAVE_API;
} /* end H5FD__tracker_vfd_open() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_close
 *
 * Purpose:     Closes an HDF5 file.
 *
 * Return:      Success:    SUCCEED
 *              Failure:    FAIL, file not closed.
 *
 *-------------------------------------------------------------------------
 */
static herr_t H5FD__tracker_vfd_close(H5FD_t *_file) {

  unsigned long t_start = get_time_usec();
  unsigned long t1, t2;
  H5FD_tracker_vfd_t *file = (H5FD_tracker_vfd_t *)_file;
  herr_t ret_value = SUCCEED; /* Return value */
  assert(file);

  /* custom VFD code start */
  unsigned long t_end = get_time_usec();
  // print_open_close_info("H5FD__tracker_vfd_close", file, file->filename, t_start, get_time_usec(), file->eof, file->flags);


  open_close_info_update("H5FD__tracker_vfd_close", file, file->eof, file->flags, t_start, t_end);
  dump_vfd_file_stat_yaml(TKR_HELPER_VFD, file->vfd_file_info);
  rm_vfd_file_node(TKR_HELPER_VFD, _file);
  

  /* custom VFD code end */


  t1 = get_time_usec();
  // close(file->fd);
  if (close(file->fd) < 0)
    // H5FD_TRACKER_VFD_SYS_GOTO_ERROR(H5E_IO, H5E_CANTCLOSEFILE, FAIL,
    //                             "unable to close file");
  t2 = get_time_usec();
  TOTAL_POSIX_IO_TIME += (t2 - t1);

  // if (file->filename) {
  //   free(file->filename);
  // }
  free(file);

done:
  H5FD_TRACKER_VFD_FUNC_LEAVE_API;
} /* end H5FD__tracker_vfd_close() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_cmp
 *
 * Purpose:     Compares two buckets belonging to this driver using an
 *              arbitrary (but consistent) ordering.
 *
 * Return:      Success:    A value like strcmp()
 *              Failure:    never fails (arguments were checked by the
 *                          caller).
 *
 *-------------------------------------------------------------------------
 */
static int H5FD__tracker_vfd_cmp(const H5FD_t *_f1, const H5FD_t *_f2) {
  const H5FD_tracker_vfd_t *f1        = (const H5FD_tracker_vfd_t *)_f1;
  const H5FD_tracker_vfd_t *f2        = (const H5FD_tracker_vfd_t *)_f2;
  int                  ret_value = 0;

  ret_value = strcmp(f1->filename, f2->filename);

  return ret_value;
} /* end H5FD__tracker_vfd_cmp() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_query
 *
 * Purpose:     Set the flags that this VFL driver is capable of supporting.
 *              (listed in H5FDpublic.h)
 *
 * Return:      SUCCEED (Can't fail)
 *
 *-------------------------------------------------------------------------
 */
static herr_t H5FD__tracker_vfd_query(const H5FD_t *_file,
                                 unsigned long *flags /* out */) {
  /* Set the VFL feature flags that this driver supports */
  /* Notice: the Mirror VFD Writer currently uses only the tracker_vfd driver as
   * the underying driver -- as such, the Mirror VFD implementation copies
   * these feature flags as its own. Any modifications made here must be
   * reflected in H5FDmirror.c
   * -- JOS 2020-01-13
   */
  herr_t ret_value = SUCCEED;

  if (flags) {
    *flags = 0;
  }                                            /* end if */

  return ret_value;
} /* end H5FD__tracker_vfd_query() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_get_eoa
 *
 * Purpose:     Gets the end-of-address marker for the file. The EOA marker
 *              is the first address past the last byte allocated in the
 *              format address space.
 *
 * Return:      The end-of-address marker.
 *
 *-------------------------------------------------------------------------
 */
static haddr_t H5FD__tracker_vfd_get_eoa(const H5FD_t *_file,
                                    H5FD_mem_t type) {
  (void) type;
  haddr_t ret_value = HADDR_UNDEF;

  const H5FD_tracker_vfd_t *file = (const H5FD_tracker_vfd_t *)_file;

  ret_value = file->eoa;

  return ret_value;
} /* end H5FD__tracker_vfd_get_eoa() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_set_eoa
 *
 * Purpose:     Set the end-of-address marker for the file. This function is
 *              called shortly after an existing HDF5 file is opened in order
 *              to tell the driver where the end of the HDF5 data is located.
 *
 * Return:      SUCCEED (Can't fail)
 *
 *-------------------------------------------------------------------------
 */
static herr_t H5FD__tracker_vfd_set_eoa(H5FD_t *_file,
                                   H5FD_mem_t type,
                                   haddr_t addr) {
  (void) type;
  herr_t ret_value = SUCCEED;

  H5FD_tracker_vfd_t *file = (H5FD_tracker_vfd_t *)_file;

  file->eoa = addr;

  return ret_value;
} /* end H5FD__tracker_vfd_set_eoa() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_get_eof
 *
 * Purpose:     Returns the end-of-file marker, which is the greater of
 *              either the filesystem end-of-file or the HDF5 end-of-address
 *              markers.
 *
 * Return:      End of file address, the first address past the end of the
 *              "file", either the filesystem file or the HDF5 file.
 *
 *-------------------------------------------------------------------------
 */
static haddr_t H5FD__tracker_vfd_get_eof(const H5FD_t *_file,
                                    H5FD_mem_t type) {
  (void) type;
  haddr_t ret_value = HADDR_UNDEF;

  const H5FD_tracker_vfd_t *file = (const H5FD_tracker_vfd_t *)_file;

  ret_value = file->eof;

  return ret_value;
} /* end H5FD__tracker_vfd_get_eof() */

/*-------------------------------------------------------------------------
 * Function:       H5FD__tracker_vfd_get_handle
 *
 * Purpose:        Returns the file handle of sec2 file driver.
 *
 * Returns:        SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__tracker_vfd_get_handle(H5FD_t *_file, hid_t H5_ATTR_UNUSED fapl, void **file_handle)
{
    H5FD_tracker_vfd_t *file      = (H5FD_tracker_vfd_t *)_file;
    herr_t       ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    if (!file_handle)
        H5FD_TRACKER_VFD_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "file handle not valid");

    *file_handle = &(file->fd);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5FD__tracker_vfd_get_handle() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_read
 *
 * Purpose:     Reads SIZE bytes of data from FILE beginning at address ADDR
 *              into buffer BUF according to data transfer properties in
 *              DXPL_ID. Determine the number of file pages affected by this
 *              call from ADDR and SIZE. Utilize transfer buffer PAGE_BUF to
 *              read the data from Blobs. Exercise care for the first and last
 *              pages to prevent overwriting existing data.
 *
 * Return:      Success:    SUCCEED. Result is stored in caller-supplied
 *                          buffer BUF.
 *              Failure:    FAIL, Contents of buffer BUF are undefined.
 *
 *-------------------------------------------------------------------------
 */
  // if (REGION_OVERFLOW(addr, size))
  //     H5FD_TRACKER_VFD_GOTO_ERROR(H5E_ARGS, H5E_OVERFLOW, FAIL,
  //                          "addr overflow, addr = %llu",
  //                          (unsigned long long)addr);

static herr_t H5FD__tracker_vfd_read(H5FD_t *_file, H5FD_mem_t type,
                                hid_t dxpl_id, haddr_t addr,
                                size_t size, void *buf) {
// #ifdef DEBUG_LOG
//   std::cout << "H5FD__read()" << std::endl;
// #endif


  unsigned long t_start = get_time_usec();
  unsigned long t1, t2, t_end;
  (void) dxpl_id; (void) type;
  H5FD_tracker_vfd_t *file = (H5FD_tracker_vfd_t *)_file;
  HDoff_t      offset    = (HDoff_t)addr;
  herr_t ret_value = SUCCEED; /* Return value */
  ssize_t count = -1;
  char file_name_copy[H5FD_MAX_FILENAME_LEN];

  assert(file && file->pub.cls);
  assert(buf);

  /* Check for overflow conditions */
  if (HADDR_UNDEF == addr)
      H5FD_TRACKER_VFD_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL,
                           "addr undefined, addr = %llu",
                           (unsigned long long)addr);
  if (REGION_OVERFLOW(addr, size))
      H5FD_TRACKER_VFD_GOTO_ERROR(H5E_ARGS, H5E_OVERFLOW, FAIL,
                           "addr overflow, addr = %llu",
                           (unsigned long long)addr);

#ifndef H5_HAVE_PREADWRITE
    /* Seek to the correct location (if we don't have pread) */
    if (addr != file->pos || OP_READ != file->op)
        if (HDlseek(file->fd, (HDoff_t)addr, SEEK_SET) < 0)
            H5FD_TRACKER_VFD_SYS_GOTO_ERROR(H5E_IO, H5E_SEEKERROR, FAIL, "unable to seek to proper position")
#endif /* H5_HAVE_PREADWRITE */

    /* Read data, being careful of interrupted system calls, partial results,
     * and the end of the file.
     */
    while (size > 0) {
        h5_posix_io_t     bytes_in   = 0;  /* # of bytes to read       */
        h5_posix_io_ret_t bytes_read = -1; /* # of bytes actually read */

        /* Trying to read more bytes than the return type can handle is
         * undefined behavior in POSIX.
         */
        if (size > H5_POSIX_MAX_IO_BYTES)
            bytes_in = H5_POSIX_MAX_IO_BYTES;
        else
            bytes_in = (h5_posix_io_t)size;

        do {
          t1 = get_time_usec();
#ifdef H5_HAVE_PREADWRITE
            bytes_read = HDpread(file->fd, buf, bytes_in, offset);
            if (bytes_read > 0)
                offset += bytes_read;
#else
            bytes_read  = HDread(file->fd, buf, bytes_in);
#endif /* H5_HAVE_PREADWRITE */
          t2 = get_time_usec();
          TOTAL_POSIX_IO_TIME += (t2 - t1);
        } while (-1 == bytes_read && EINTR == errno);

        if (-1 == bytes_read) { /* error */
            int    myerrno = errno;
            time_t mytime  = HDtime(NULL);

            t1 = get_time_usec();
            offset = HDlseek(file->fd, (HDoff_t)0, SEEK_CUR);
            t2 = get_time_usec();
            TOTAL_POSIX_IO_TIME += (t2 - t1);

            H5FD_TRACKER_VFD_GOTO_ERROR(H5E_IO, H5E_READERROR, FAIL,
                        "file read failed: time = %s, filename = '%s', file descriptor = %d, errno = %d, "
                        "error message = '%s', buf = %p, total read size = %llu, bytes this sub-read = %llu, "
                        "bytes actually read = %llu, offset = %llu",
                        HDctime(&mytime), file->filename, file->fd, myerrno, strerror(myerrno), buf,
                        (unsigned long long)size, (unsigned long long)bytes_in,
                        (unsigned long long)bytes_read, (unsigned long long)offset);
        } /* end if */

        if (0 == bytes_read) {
            /* end of file but not end of format address space */
            memset(buf, 0, size);
            break;
        } /* end if */

        assert(bytes_read >= 0);
        assert((size_t)bytes_read <= size);

        size -= (size_t)bytes_read;
        addr += (haddr_t)bytes_read;
        buf = (char *)buf + bytes_read;
    } /* end while */

    /* Update current position */
    file->pos = addr;
    file->op  = OP_READ;

  /* custom VFD code start */
  t_end = get_time_usec();

  // // make a copy of the file name
  // file_name_copy[0] = '\0';
  // strcat(file_name_copy, file->filename);

  read_write_info_update("H5FD__tracker_vfd_read", file->filename, file->my_fapl_id ,_file,
    type, dxpl_id, addr, size, file->page_size, t_start, t_end);

  // print_read_write_info("H5FD__tracker_vfd_read", file->filename, file->my_fapl_id ,_file,
  //   type, dxpl_id, addr, size, file->page_size, t_start, t_end);

  /* custom VFD code end */

done:
  if (ret_value < 0) {
      /* Reset last file I/O information */
      file->pos = HADDR_UNDEF;
      file->op  = OP_UNKNOWN;
  } /* end if */

  H5FD_TRACKER_VFD_FUNC_LEAVE_API;

  // return ret_value;
} /* end H5FD__tracker_vfd_read() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_write
 *
 * Purpose:     Writes SIZE bytes of data to FILE beginning at address ADDR
 *              from buffer BUF according to data transfer properties in
 *              DXPL_ID.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t H5FD__tracker_vfd_write(H5FD_t *_file, H5FD_mem_t type,
                                 hid_t dxpl_id, haddr_t addr,
                                 size_t size, const void *buf) {
#ifdef DEBUG_LOG
  std::cout << "H5FD__write()" << std::endl;
#endif

  unsigned long t_start = get_time_usec();
  unsigned long t1, t2, t_end;
  (void) dxpl_id; (void) type;
  H5FD_tracker_vfd_t *file = (H5FD_tracker_vfd_t *)_file;
  HDoff_t      offset    = (HDoff_t)addr;
  herr_t ret_value = SUCCEED;


#ifndef H5_HAVE_PREADWRITE
    /* Seek to the correct location (if we don't have pwrite) */
    if (addr != file->pos || OP_WRITE != file->op)
        if (HDlseek(file->fd, (HDoff_t)addr, SEEK_SET) < 0)
            H5FD_TRACKER_VFD_SYS_GOTO_ERROR(H5E_IO, H5E_SEEKERROR, FAIL, "unable to seek to proper position")
#endif /* H5_HAVE_PREADWRITE */
    
    /* Write the data, being careful of interrupted system calls and partial
     * results
     */
    while (size > 0) {
        h5_posix_io_t     bytes_in    = 0;  /* # of bytes to write  */
        h5_posix_io_ret_t bytes_wrote = -1; /* # of bytes written   */

        /* Trying to write more bytes than the return type can handle is
         * undefined behavior in POSIX.
         */
        if (size > H5_POSIX_MAX_IO_BYTES)
            bytes_in = H5_POSIX_MAX_IO_BYTES;
        else
            bytes_in = (h5_posix_io_t)size;

        do {
          t1 = get_time_usec();
#ifdef H5_HAVE_PREADWRITE
            bytes_wrote = HDpwrite(file->fd, buf, bytes_in, offset);
            if (bytes_wrote > 0)
                offset += bytes_wrote;
#else
            bytes_wrote = HDwrite(file->fd, buf, bytes_in);
#endif /* H5_HAVE_PREADWRITE */
          t2 = get_time_usec();
          TOTAL_POSIX_IO_TIME += (t2 - t1);
        } while (-1 == bytes_wrote && EINTR == errno);

        if (-1 == bytes_wrote) { /* error */
            int    myerrno = errno;
            time_t mytime  = HDtime(NULL);

            t1 = get_time_usec();
            offset = HDlseek(file->fd, (HDoff_t)0, SEEK_CUR);
            t2 = get_time_usec();
            TOTAL_POSIX_IO_TIME += (t2 - t1);

            H5FD_TRACKER_VFD_GOTO_ERROR(H5E_IO, H5E_WRITEERROR, FAIL,
                        "file write failed: time = %s, filename = '%s', file descriptor = %d, errno = %d, "
                        "error message = '%s', buf = %p, total write size = %llu, bytes this sub-write = "
                        "%llu, bytes actually written = %llu, offset = %llu",
                        HDctime(&mytime), file->filename, file->fd, myerrno, strerror(myerrno), buf,
                        (unsigned long long)size, (unsigned long long)bytes_in,
                        (unsigned long long)bytes_wrote, (unsigned long long)offset);
        } /* end if */

        assert(bytes_wrote > 0);
        assert((size_t)bytes_wrote <= size);

        size -= (size_t)bytes_wrote;
        addr += (haddr_t)bytes_wrote;
        buf = (const char *)buf + bytes_wrote;
    } /* end while */

    /* Update current position and eof */
    file->pos = addr;
    file->op  = OP_WRITE;
    if (file->pos > file->eof)
        file->eof = file->pos;
  

  /* custom VFD code start */
  t_end = get_time_usec();
  read_write_info_update("H5FD__tracker_vfd_write", file->filename, file->my_fapl_id ,_file,
    type, dxpl_id, addr, size, file->page_size, t_start, t_end);


  // print_read_write_info("H5FD__tracker_vfd_write", file->filename, file->my_fapl_id ,_file,
  //   type, dxpl_id, addr, size, file->page_size, t_start, t_end);
  
  /* custom VFD code end */

done:
  if (ret_value < 0) {
    /* Reset last file I/O information */
    file->pos = HADDR_UNDEF;
    file->op  = OP_UNKNOWN;
  } /* end if */

  H5FD_TRACKER_VFD_FUNC_LEAVE_API;
} /* end H5FD__tracker_vfd_write() */


/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_truncate
 *
 * Purpose:     Makes sure that the true file size is the same (or larger)
 *              than the end-of-address.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__tracker_vfd_truncate(H5FD_t *_file, hid_t H5_ATTR_UNUSED dxpl_id, bool H5_ATTR_UNUSED closing)
{
    H5FD_tracker_vfd_t *file      = (H5FD_tracker_vfd_t *)_file;
    herr_t       ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(file);

    /* Extend the file to make sure it's large enough */
    if (!H5_addr_eq(file->eoa, file->eof)) {

        if (-1 == HDftruncate(file->fd, (HDoff_t)file->eoa))
            H5FD_TRACKER_VFD_SYS_GOTO_ERROR(H5E_IO, H5E_SEEKERROR, FAIL, "unable to extend file properly");

        /* Update the eof value */
        file->eof = file->eoa;

        /* Reset last file I/O information */
        file->pos = HADDR_UNDEF;
        file->op  = OP_UNKNOWN;
    } /* end if */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5FD__tracker_vfd_truncate() */


/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_lock
 *
 * Purpose:     To place an advisory lock on a file.
 *		The lock type to apply depends on the parameter "rw":
 *			true--opens for write: an exclusive lock
 *			false--opens for read: a shared lock
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__tracker_vfd_lock(H5FD_t *_file, bool rw)
{
    H5FD_tracker_vfd_t *file = (H5FD_tracker_vfd_t *)_file; /* VFD file struct          */
    int          lock_flags;                  /* file locking flags       */
    herr_t       ret_value = SUCCEED;         /* Return value             */

    FUNC_ENTER_PACKAGE

    assert(file);

    /* Set exclusive or shared lock based on rw status */
    lock_flags = rw ? LOCK_EX : LOCK_SH;

    /* Place a non-blocking lock on the file */
    if (HDflock(file->fd, lock_flags | LOCK_NB) < 0) {
        if (file->ignore_disabled_file_locks && ENOSYS == errno) {
            /* When errno is set to ENOSYS, the file system does not support
             * locking, so ignore it.
             */
            errno = 0;
        }
        else
            H5FD_TRACKER_VFD_SYS_GOTO_ERROR(H5E_VFL, H5E_CANTLOCKFILE, FAIL, "unable to lock file");
    }

done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5FD__tracker_vfd_lock() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_unlock
 *
 * Purpose:     To remove the existing lock on the file
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__tracker_vfd_unlock(H5FD_t *_file)
{
    H5FD_tracker_vfd_t *file      = (H5FD_tracker_vfd_t *)_file; /* VFD file struct          */
    herr_t       ret_value = SUCCEED;              /* Return value             */

    FUNC_ENTER_PACKAGE

    assert(file);

    if (HDflock(file->fd, LOCK_UN) < 0) {
        if (file->ignore_disabled_file_locks && ENOSYS == errno) {
            /* When errno is set to ENOSYS, the file system does not support
             * locking, so ignore it.
             */
            errno = 0;
        }
        else
            H5FD_TRACKER_VFD_SYS_GOTO_ERROR(H5E_VFL, H5E_CANTUNLOCKFILE, FAIL, "unable to unlock file");
    }

done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5FD__tracker_vfd_unlock() */


/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_delete
 *
 * Purpose:     Delete a file
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__tracker_vfd_delete(const char *filename, hid_t H5_ATTR_UNUSED fapl_id)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    assert(filename);

    if (HDremove(filename) < 0)
        H5FD_TRACKER_VFD_SYS_GOTO_ERROR(H5E_VFL, H5E_CANTDELETEFILE, FAIL, "unable to delete file");

done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5FD__tracker_vfd_delete() */

/*-------------------------------------------------------------------------
 * Function:    H5FD__tracker_vfd_ctl
 *
 * Purpose:     Sec2 VFD version of the ctl callback.
 *
 *              The desired operation is specified by the op_code
 *              parameter.
 *
 *              The flags parameter controls management of op_codes that
 *              are unknown to the callback
 *
 *              The input and output parameters allow op_code specific
 *              input and output
 *
 *              At present, no op codes are supported by this VFD.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD__tracker_vfd_ctl(H5FD_t H5_ATTR_UNUSED *_file, uint64_t H5_ATTR_UNUSED op_code, uint64_t flags,
               const void H5_ATTR_UNUSED *input, void H5_ATTR_UNUSED **output)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* No op codes are understood. */
    if (flags & H5FD_CTL_FAIL_IF_UNKNOWN_FLAG)
        H5FD_TRACKER_VFD_GOTO_ERROR(H5E_VFL, H5E_FCNTL, FAIL, "unknown op_code and fail if unknown flag is set");

done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5FD__tracker_vfd_ctl() */

/*
 * Stub routines for dynamic plugin loading
 */
H5PL_type_t
H5PLget_plugin_type(void) {
  return H5PL_TYPE_VFD;
}

const void*
H5PLget_plugin_info(void) {
  return &H5FD_tracker_vfd_g;
}

//}  // extern C





/** Only Initialize MPI if it hasn't already been initialized. */
// int MPI_Init(int *argc, char ***argv) {
//   int status = MPI_SUCCESS;
//   if (mpi_is_initialized == FALSE) {
//     int status = PMPI_Init(argc, argv);
//     if (status == MPI_SUCCESS) {
//       mpi_is_initialized = TRUE;
//     }
//   }

//   return status;
// }

// int MPI_Finalize() {
//   MAP_OR_FAIL(H5_term_library);
//   real_H5_term_library_();

//   if (tracker_vfd_initialized == TRUE) {
//     tracker_vfd_initialized = FALSE;
//   }

//   int status = PMPI_Finalize();
//   if (status == MPI_SUCCESS) {
//     mpi_is_initialized = FALSE;
//   }

//   return status;
// }