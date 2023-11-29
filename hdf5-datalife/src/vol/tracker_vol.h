/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Purpose:	The public header file for the pass-through VOL connector.
 */

#ifndef _tracker_vol_H
#define _tracker_vol_H

/* Public headers needed by this file */
#include "H5VLpublic.h"        /* Virtual Object Layer                 */

/* Identifier for the pass-through VOL connector */
#define H5VL_PROVNC	(H5VL_tracker_register())

/* Characteristics of the pass-through VOL connector */
#define H5VL_TRACKER_NAME        "tracker"
#define H5VL_TRACKER_VALUE       909           /* VOL connector ID */
#define H5VL_TRACKER_VERSION     1

#define SUCCEED 0
#define FAIL    (-1)
#define H5_REQUEST_NULL NULL
#define DEFAULT_PAGE_SIZE 8192

typedef enum TrackLevel {
    Default, //no file write, only screen print
    Print_only,
    File_only,
    File_and_print,
    Level4,
    Level5,
    Disabled
}Track_level;

/* Pass-through VOL connector info */
typedef struct H5VL_tracker_info_t {
    hid_t under_vol_id;         /* VOL ID for under VOL */
    void *under_vol_info;       /* VOL info for under VOL */
    char* tkr_file_path;
    Track_level tkr_level;
    char* tkr_line_format;
} H5VL_tracker_info_t;


#ifdef __cplusplus
extern "C" {
#endif

H5_DLL hid_t H5VL_tracker_register(void);

#ifdef __cplusplus
}
#endif

#endif /* _tracker_vol_H */

