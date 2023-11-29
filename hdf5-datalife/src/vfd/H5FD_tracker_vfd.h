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
 * Programmer:  Kimmy Mu
 *              April 2021
 *
 * Purpose: The public header file for the Tracker VFD driver.
 */
#ifndef H5FDtracker_VFD
#define H5FDtracker_VFD

#include <dlfcn.h>
#include <hdf5.h>
#include <stdio.h>

#define H5FD_TRACKER_VFD_NAME  "hdf5_tracker_vfd"
#define H5FD_TRACKER_VFD_VALUE ((H5FD_class_value_t)(515))

#define TRACKER_VFD_FORWARD_DECL(func_, ret_, args_) \
  typedef ret_(*real_t_##func_##_) args_;       \
  ret_(*real_##func_##_) args_ = NULL;

#define MAP_OR_FAIL(func_)                                                  \
  if (!(real_##func_##_)) {                                                 \
    real_##func_##_ = (real_t_##func_##_)dlsym(RTLD_NEXT, #func_);          \
    if (!(real_##func_##_)) {                                               \
      fprintf(stderr, "TRACKER_VFD Adapter failed to map symbol: %s\n", #func_); \
      exit(1);                                                              \
    }                                                                       \
  }

#ifdef __cplusplus
extern "C" {
#endif

hid_t H5FD_tracker_vfd_init();
herr_t H5Pset_fapl_tracker_vfd(hid_t fapl_id, hbool_t persistence, size_t page_size);

H5PL_type_t H5PLget_plugin_type(void);
const void* H5PLget_plugin_info(void);

TRACKER_VFD_FORWARD_DECL(H5_init_library, herr_t, ());
TRACKER_VFD_FORWARD_DECL(H5_term_library, herr_t, ());

#ifdef __cplusplus
}
#endif

#endif /* end H5FDtracker_VFD */
