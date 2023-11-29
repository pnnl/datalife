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
 *              Sep 2022
 *
 * Purpose: The tracker_vfd file driver using only the HDF5 public API
 *          and buffer datasets in Tracker VFD buffering systems with
 *          multiple storage tiers.
 */
#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <cstring>
#include <assert.h>
#include <math.h>


#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>

// #include <mpi.h>

/* HDF5 header for dynamic plugin loading */
// #include "H5PLextern.h"
#include "H5FD_tracker_vfd.h"     /* Tracker VFD file driver     */
// #include "H5FD_tracker_vfd_err.h" /* TODO: error handling         */


// #ifdef ENABLE_TRACKER
#include "../vol/tracker_vol_types.h" /* Connecting to vol */
// #include "tracker_vol_types.h" /* only VFD */


#include <time.h>       // for struct timespec, clock_gettime(CLOCK_MONOTONIC, &end);

// For extracing runtime command
#ifdef DEBUG_LOG
#include <iostream>
#endif

#include <fstream>
#include <sstream>
#include <vector>
#include <dirent.h>
#include <fcntl.h> // For flags like O_RDONLY
/* candice added functions for I/O traces end */

#define MAX_FILE_INTENT_LENGTH 128
#define MAX_CONF_STR_LENGTH 128
#define H5FD_MAX_FILENAME_LEN 1024 // same as H5FD_MAX_FILENAME_LEN

static
unsigned long get_time_usec(void) {
    struct timeval tp;

    gettimeofday(&tp, nullptr);
    return (unsigned long)((1000000 * tp.tv_sec) + tp.tv_usec);
}


/************/
/* Typedefs */
/************/
typedef struct H5FD_tkr_file_info_t vfd_file_tkr_info_t;
unsigned long TOTAL_TKR_VFD_OVERHEAD;
unsigned long TOTAL_POSIX_IO_TIME;
std::string read_func = "H5FD__tracker_vfd_read";
std::string write_func = "H5FD__tracker_vfd_write";


typedef struct VFDTrackerHelper {
    /* VFDTrackerHelper properties */
    char* tkr_file_path;
    FILE* tkr_file_handle;
    hbool_t logStat;
    char user_name[32];
    int pid;
    pthread_t tid;
    char proc_name[64];
    int ptr_cnt;
    int vfd_opened_files_cnt;
    vfd_file_tkr_info_t* vfd_opened_files;//linkedlist,
    size_t tracker_vfd_page_size;

} vfd_tkr_helper_t;

static vfd_tkr_helper_t* TKR_HELPER_VFD = nullptr;


struct page_range_t {
    size_t start_page;
    size_t end_page;
    int io_idx;
    page_range_t* next;
};

struct h5_mem_stat_t {
    std::string mem_type;
    size_t read_bytes;
    size_t write_bytes;

    int read_cnt;
    page_range_t* read_ranges; // linked-list head pointer
    page_range_t* read_ranges_tail; // linked-list tail pointer

    int write_cnt;
    page_range_t* write_ranges; // linked-list head pointer
    page_range_t* write_ranges_tail; // linked-list tail pointer
};

struct H5FD_tkr_file_info_t { // used by VFD
    vfd_tkr_helper_t* vfd_tkr_helper;  //pointer shared among all layers, one per process.

    const char* file_name;
    unsigned long file_no;
    char* intent;
    char * task_name;
    unsigned long sorder_id; // need lock
    hid_t fapl_id;
    int file_read_cnt;
    int file_write_cnt;
    size_t file_size;
    size_t adaptor_page_size;
    size_t io_bytes;

    /* common metadata access type */
    h5_mem_stat_t * h5_draw; // H5FD_MEM_DRAW
    h5_mem_stat_t * h5_ohdr; // H5FD_MEM_OHDR
    h5_mem_stat_t * h5_super; // H5FD_MEM_SUPER 
    h5_mem_stat_t * h5_btree; // H5FD_MEM_BTREE
    h5_mem_stat_t * h5_lheap; // H5FD_MEM_LHEAP
    
    int ref_cnt;
    unsigned long open_time;
    unsigned long close_time;

    /* beloe types not added 
    H5FD_MEM_DEFAULT, 
    H5FD_MEM_GHEAP, 
    H5FD_MEM_NTYPES, 
    H5FD_MEM_NOLIST
    */

    vfd_file_tkr_info_t *next;
};


/* The description of a file belonging to this driver. */
typedef struct H5FD_tracker_vfd_t {
    H5FD_t         pub; /* public stuff, must be first      */
    int            fd;  /* the filesystem file descriptor   */
    haddr_t        eoa; /* end of allocated region          */
    haddr_t        eof; /* end of file; current file size   */
    haddr_t        pos; /* current file I/O position        */
    int op;  /* last operation                   */
    bool           ignore_disabled_file_locks;
    char           filename[H5FD_MAX_FILENAME_LEN]; /* Copy of file name from open operation */
    bool fam_to_single;
    unsigned       flags;       /* The flags passed from H5Fcreate/H5Fopen */

  /* custom VFD code start */
  hbool_t        logStat; /* write I/O stats to yaml file           */
  size_t         page_size;   /* page size */
  hid_t          my_fapl_id;     /* file access property list ID */
  vfd_tkr_helper_t *vfd_tkr_helper; /* pointer shared among all layers, one per process. */
  vfd_file_tkr_info_t * vfd_file_info; /* file info */
  // std::string logFilePath; /* log file path */

  /* custom VFD code end */

} H5FD_tracker_vfd_t;

/* function prototypes*/
std::string getFileIntentFlagsStr(unsigned int flags);
void update_mem_type_stat_helper(int rw, size_t start_page, 
  size_t end_page, size_t access_size, h5_mem_stat_t* mem_stat);
void update_mem_type_stat(int rw, size_t start_page, 
  size_t end_page, size_t access_size, H5FD_mem_t type, vfd_file_tkr_info_t * info);
void read_write_info_update(std::string func_name, char * file_name, hid_t fapl_id, H5FD_t *_file,
  H5FD_mem_t type, hid_t dxpl_id, haddr_t addr,
  size_t size, size_t page_size, unsigned long t_start, unsigned long t_end);

void open_close_info_update(const char* func_name, H5FD_tracker_vfd_t *file, size_t eof, int flags, 
  unsigned long t_start, unsigned long t_end);
void print_open_close_info(const char* func_name, void * obj, const char * file_name, 
  size_t eof, int flags, unsigned long t_start, unsigned long t_end);


void dump_vfd_file_stat_yaml(vfd_tkr_helper_t* helper, const vfd_file_tkr_info_t* info);
void dump_vfd_mem_stat_yaml(FILE* f, const h5_mem_stat_t* mem_stat);

void parseEnvironmentVariable(char* file_path);
vfd_tkr_helper_t * vfd_tkr_helper_init( char* file_path, size_t page_size, hbool_t logStat);
void vfd_file_info_free(vfd_file_tkr_info_t* info);
vfd_file_tkr_info_t* new_vfd_file_info(const char* fname, unsigned long file_no);
vfd_file_tkr_info_t* add_vfd_file_node(vfd_tkr_helper_t * helper, const char* file_name, void * obj);
int rm_vfd_file_node(vfd_tkr_helper_t* helper, H5FD_t *_file);
void vfd_tkr_helper_teardown(vfd_tkr_helper_t* helper);


std::string get_ohdr_type(H5F_mem_t type);
std::string get_mem_type(H5F_mem_t type);

// Function to extract the pipe_handle number from the command line string
bool extractPipeHandle(const std::string& commandLine, int& pipeHandle) {
    size_t pipeHandlePos = commandLine.find("pipe_handle=");
    if (pipeHandlePos != std::string::npos) {
        // Extract the substring starting from 'pipe_handle=' to the next space or end of the string
        std::string pipeHandleStr = commandLine.substr(pipeHandlePos + 11); // 11 is the length of "pipe_handle="
        size_t spacePos = pipeHandleStr.find(' ');
        if (spacePos != std::string::npos) {
            pipeHandleStr = pipeHandleStr.substr(0, spacePos);
        }
        try {
            pipeHandle = std::stoi(pipeHandleStr);
            return true;
        } catch (const std::exception& e) {
            printf("pipe_handle: %s\n", pipeHandleStr.c_str());
        }
    }
    return false;
}

bool getCurrentTaskFromFile(std::string& curr_task) {
    // Get environment variables
    const char* path_for_task_files = std::getenv("PATH_FOR_TASK_FILES");
    const char* workflow_name = std::getenv("WORKFLOW_NAME");

    if (path_for_task_files && workflow_name) {
        // Construct the full file path
        std::string file_path = std::string(path_for_task_files) + "/" + std::string(workflow_name) + "_vfd.curr_task";

        // Open the file with exclusive lock
        int fd = open(file_path.c_str(), O_RDONLY);
        if (fd != -1) {
            struct flock lock;
            lock.l_type = F_RDLCK;
            lock.l_start = 0;
            lock.l_whence = SEEK_SET;
            lock.l_len = 0;

            // Attempt to obtain a shared lock on the file
            if (fcntl(fd, F_SETLK, &lock) != -1) {
                // Read the file content into a string
                std::ifstream file_stream(file_path);
                if (file_stream) {
                    std::stringstream buffer;
                    buffer << file_stream.rdbuf();
                    curr_task = buffer.str();

                    close(fd);  // Release the lock
                    return true;
                } else {
                    printf("Failed to read file: %s\n", file_path.c_str());
                }
            } else {
                printf("Failed to obtain lock on file: %s\n", file_path.c_str());
            }
            close(fd);
        } else {
            printf("Failed to open file: %s - %s\n", file_path.c_str(), strerror(errno));
        }
    } else {
        printf("Missing environment variables: PATH_FOR_TASK_FILES and/or WORKFLOW_NAME\n");
    }

    return false;
}

std::string getTaskName() {
    // Check if the environment variable CURR_TASK is set
    const char* curr_task_env = std::getenv("CURR_TASK");
    
    if (curr_task_env) {
        return std::string(curr_task_env) + "-" + std::to_string(getpid());
    } else {
        // If CURR_TASK is not set, try reading it from a file
        std::string curr_task;
        if (getCurrentTaskFromFile(curr_task)) {
            return curr_task + "-" + std::to_string(getpid());
        } else {
            // If both methods fail, return an empty string
            return "";
        }
    }
}

// Function to get the full command line for a given PID
bool getCommandLineByPID(int pid, std::string& commandLine) {
    std::string procDir = "/proc/" + std::to_string(pid) + "/cmdline";
    std::ifstream cmdlineFile(procDir);

    if (cmdlineFile.is_open()) {
        std::stringstream buffer;
        buffer << cmdlineFile.rdbuf();
        commandLine = buffer.str();

        // Replace null bytes with spaces in the command line
        size_t nullBytePos;
        while ((nullBytePos = commandLine.find('\0')) != std::string::npos) {
            commandLine.replace(nullBytePos, 1, " ");
        }

        return true;
    } else {
        printf("Failed to open cmdline file for PID %d\n", pid);
        return false;
    }
}


void parseEnvironmentVariable(char* file_path) {
    const char* path_str = std::getenv("HDF5_LOG_FILE_PATH");
    if (path_str) {
        // Find the first occurrence of ';' or the end of the string
        const char* delimiter = strchr(path_str, ';');
        if (delimiter) {
            // Calculate the length of the path
            size_t path_length = delimiter - path_str;
            
            // Add "vfd-" as a prefix to the path and copy to file_path
            strncpy(file_path, "vfd-", sizeof("vfd-") - 1);
            strncat(file_path, path_str, path_length);
            
            // Null-terminate the file_path
            file_path[sizeof("vfd-") - 1 + path_length] = '\0';
        } else {
            // No delimiter found, so use the entire string and add "vfd-" prefix
            strcpy(file_path, "vfd-");
            strcat(file_path, path_str);
        }
    } else {
        // Set a default file path
        strcpy(file_path, "vfd-data-stat.yaml");
    }
}



/* candice added, print H5FD_mem_t H5FD_MEM_OHDR type more info */
std::string get_ohdr_type(H5F_mem_t type){
// char * get_ohdr_type(H5F_mem_t type){

  if (type == H5FD_MEM_FHEAP_HDR){
    // printf("- Access_Region_Mem_Type : H5FD_MEM_FHEAP_HDR \n");
    return "H5FD_MEM_FHEAP_HDR";

  } else if( type == H5FD_MEM_FHEAP_IBLOCK ){
    // printf("- Access_Region_Mem_Type : H5FD_MEM_FHEAP_IBLOCK \n");
    return "H5FD_MEM_FHEAP_IBLOCK";

  } else if( type == H5FD_MEM_FSPACE_HDR ){
    // printf("- Access_Region_Mem_Type : H5FD_MEM_FSPACE_HDR \n");
    return "H5FD_MEM_FSPACE_HDR";

  } else if( type == H5FD_MEM_SOHM_TABLE  ){
    // printf("- Access_Region_Mem_Type : H5FD_MEM_SOHM_TABLE  \n");
    return "H5FD_MEM_SOHM_TABLE";

  } else if( type == H5FD_MEM_EARRAY_HDR ){
    // printf("- Access_Region_Mem_Type : H5FD_MEM_EARRAY_HDR \n");
    return "H5FD_MEM_EARRAY_HDR";

  } else if( type == H5FD_MEM_EARRAY_IBLOCK ){
    // printf("- Access_Region_Mem_Type : H5FD_MEM_EARRAY_IBLOCK \n");
    return "H5FD_MEM_EARRAY_IBLOCK";

  } else if( type == H5FD_MEM_FARRAY_HDR  ){
    // printf("- Access_Region_Mem_Type : H5FD_MEM_FARRAY_HDR  \n");
    return "H5FD_MEM_FARRAY_HDR";

  } else {
    // printf("- Access_Region_Mem_Type : H5FD_MEM_OHDR \n");
    return "H5FD_MEM_OHDR";
  }
}

std::string get_mem_type(H5F_mem_t type){
// char * get_mem_type(H5F_mem_t type){
  switch(type) {
    case H5FD_MEM_DEFAULT:
      return "H5FD_MEM_DEFAULT";
    case H5FD_MEM_SUPER:
      return "H5FD_MEM_SUPER";
    case H5FD_MEM_BTREE:
      return "H5FD_MEM_BTREE";
    case H5FD_MEM_DRAW:
      return "H5FD_MEM_DRAW";
    case H5FD_MEM_GHEAP:
      return "H5FD_MEM_GHEAP";
    case H5FD_MEM_LHEAP:
      return "H5FD_MEM_LHEAP";
    case H5FD_MEM_NTYPES:
      return "H5FD_MEM_NTYPES";
    case H5FD_MEM_NOLIST:
      return "H5FD_MEM_NOLIST";
    case H5FD_MEM_OHDR:
      return "H5FD_MEM_OHDR";
    default:
      return "H5FD_MEM_UNKNOWN";
  }
}

// for debug
void print_mem_stat(const h5_mem_stat_t* mem_stat)
{
    printf("  - %s:\n", mem_stat->mem_type.c_str());
    printf("      read_bytes: %zu\n", mem_stat->read_bytes);
    printf("      read_cnt: %d\n", mem_stat->read_cnt);
    printf("      read_ranges: {");
    
    page_range_t* read_range = mem_stat->read_ranges;
    while (read_range != nullptr) {
        printf("%d:(%zu,%zu)", read_range->io_idx, read_range->start_page, read_range->end_page);
        read_range = read_range->next;
        if (read_range != nullptr) {
            printf(",");
        }
    }
    
    printf("}\n");
    printf("      write_bytes: %zu\n", mem_stat->write_bytes);
    printf("      write_cnt: %d\n", mem_stat->write_cnt);
    printf("      write_ranges: {");
    
    page_range_t* write_range = mem_stat->write_ranges;
    while (write_range != nullptr) {
        printf("%d:(%zu,%zu)", write_range->io_idx, write_range->start_page, write_range->end_page);
        write_range = write_range->next;
        if (write_range != nullptr) {
            printf(",");
        }
    }
    
    printf("}\n");
}



void dump_vfd_mem_stat_yaml(FILE* f, const h5_mem_stat_t* mem_stat) {

    fprintf(f, "      %s:\n", mem_stat->mem_type.c_str());
    fprintf(f, "        read_bytes: %zu\n", mem_stat->read_bytes);
    fprintf(f, "        read_cnt: %d\n", mem_stat->read_cnt);
    fprintf(f, "        read_ranges: {");
    
    page_range_t* read_range = mem_stat->read_ranges;
    while (read_range != nullptr) {
        fprintf(f, "%d:[%zu,%zu]", read_range->io_idx, read_range->start_page, read_range->end_page);
        read_range = read_range->next;
        if (read_range != nullptr) {
            fprintf(f, ",");
        }
    }
    
    fprintf(f, "}\n");
    fprintf(f, "        write_bytes: %zu\n", mem_stat->write_bytes);
    fprintf(f, "        write_cnt: %d\n", mem_stat->write_cnt);
    fprintf(f, "        write_ranges: {");
    
    page_range_t* write_range = mem_stat->write_ranges;
    while (write_range != nullptr) {
        // fprintf(f, "(%zu,%zu)", write_range->start_page, write_range->end_page);
        fprintf(f, "%d:[%zu,%zu]", write_range->io_idx, write_range->start_page, write_range->end_page);
        write_range = write_range->next;
        if (write_range != nullptr) {
            fprintf(f, ",");
        }
    }
    
    fprintf(f, "}\n");
}



void dump_vfd_file_stat_yaml(vfd_tkr_helper_t* helper, const vfd_file_tkr_info_t* info) {

  printf("File close and write to : %s\n", helper->tkr_file_path);

  // const char* file_name = strrchr(info->file_name, '/');
  // Keep complete file path name
  const char * file_name = (char*)info->file_name;

  if(file_name)
      file_name++;
  else
      file_name = (const char*)info->file_name;
  
  FILE * f = fopen(helper->tkr_file_path, "a");

  if (!info) {
      fprintf(f, "dump_vfd_file_stat_yaml(): vfd_file_tkr_info_t is nullptr.\n");
      return;
  }


  fprintf(f, "- file-%lu:\n", info->sorder_id);
  fprintf(f, "    file_name: \"/%s\"\n", file_name);

  if (info->task_name != nullptr) {
      fprintf(f, "    task_name: \"%s\"\n", info->task_name);
  } else {
    // add task name
    // const char* curr_task = std::getenv("CURR_TASK");
    // // info->task_name = curr_task ? strdup(curr_task) : nullptr;
    std::string curr_task;
    std::string task_name = curr_task + "-" + std::to_string(getpid());
    fprintf(f, "    task_name: \"%s\"\n", task_name.c_str());

  }
  
  fprintf(f, "    open_time: %lu\n", info->open_time);
  fprintf(f, "    close_time: %lu\n", get_time_usec());
  fprintf(f, "    file_intent: [");
  if (info->intent != nullptr) {
      fprintf(f, "%s", info->intent);
  }
  fprintf(f, "]\n");

  fprintf(f, "    file_no: %lu\n", info->file_no);
  fprintf(f, "    file_read_cnt: %d\n", info->file_read_cnt);
  fprintf(f, "    file_write_cnt: %d\n", info->file_write_cnt);
  if(info->file_read_cnt > 0 && info->file_write_cnt == 0){
    fprintf(f, "    access_type: read_only\n");
    fprintf(f, "    file_type: input\n");
  }
  else if(info->file_write_cnt > 0 && info->file_read_cnt == 0){
    fprintf(f, "    access_type: write_only\n");
    fprintf(f, "    file_type: output\n");
  }
  else if (info->file_write_cnt > 0 && info->file_read_cnt > 0){
    // read_write does not identify order of read and write
    fprintf(f, "    access_type: read_write\n");
    fprintf(f, "    file_type: input-output\n");
  } else {
    fprintf(f, "    access_type: not_accessed\n");
    fprintf(f, "    file_type: na\n");
  }
  fprintf(f, "    total_io_bytes: %ld\n", info->io_bytes);
  fprintf(f, "    file_size: %zu\n", info->file_size);
  fprintf(f, "    data:\n");
  // Check and print h5_mem_stat_t structs if they exist
  if (info->h5_draw != nullptr) {
    dump_vfd_mem_stat_yaml(f, info->h5_draw);
  }
  fprintf(f, "    metadata:\n");
  if (info->h5_ohdr != nullptr) {
    dump_vfd_mem_stat_yaml(f, info->h5_ohdr);
  }
  if (info->h5_super != nullptr) {
    dump_vfd_mem_stat_yaml(f, info->h5_super);
  }
  if (info->h5_btree != nullptr) {
    dump_vfd_mem_stat_yaml(f, info->h5_btree);
  }
  if (info->h5_lheap != nullptr) {
    dump_vfd_mem_stat_yaml(f, info->h5_lheap);
  }
  // Print other h5_mem_stat_t structs if they exist in a similar way

  fprintf(f, "- Task:\n");
  fprintf(f, "  task_id: %d\n", getpid());
  fprintf(f, "  tracker_vfd_page_size: %ld\n", info->adaptor_page_size);
  fprintf(f, "  VFD-Total-Overhead(ms): %ld\n", TOTAL_TKR_VFD_OVERHEAD/1000);
  fprintf(f, "  POSIX-Total-IO-Time(ms): %ld\n", TOTAL_POSIX_IO_TIME/1000);

  fprintf(f, "\n");


  fflush(f);
  fclose(f);

}




std::string getFileIntentFlagsStr(unsigned int flags) {
    std::string intentFlagsStr;

    if (flags == 0)
        intentFlagsStr += "\"NOT_SPECIFIED\",";
    else{
      if (flags & H5F_ACC_RDWR)
          intentFlagsStr += "\"H5F_ACC_RDWR\",";
      if (flags & H5F_ACC_RDONLY)
          intentFlagsStr += "\"H5F_ACC_RDONLY\",";
      if (flags & H5F_ACC_TRUNC)
          intentFlagsStr += "\"H5F_ACC_TRUNC\",";
      if (flags & H5F_ACC_CREAT)
          intentFlagsStr += "\"H5F_ACC_CREAT\",";
      if (flags & H5F_ACC_EXCL)
          intentFlagsStr += "\"H5F_ACC_EXCL\",";
    }

    // Remove the trailing comma if there are any flags
    if (!intentFlagsStr.empty())
        intentFlagsStr.pop_back();
    
    return intentFlagsStr;
}







void update_mem_type_stat_helper(int rw, size_t start_page, 
  size_t end_page, size_t access_size, h5_mem_stat_t* mem_stat) 
{
    if (rw == 1) {
        page_range_t* new_range = new page_range_t();
        new_range->start_page = start_page;
        new_range->end_page = end_page;
        new_range->io_idx = VFD_ACCESS_IDX;
        new_range->next = nullptr;

        if (mem_stat->read_ranges == nullptr) {
            mem_stat->read_ranges = new_range;
            mem_stat->read_ranges_tail = new_range;
        } else {
            mem_stat->read_ranges_tail->next = new_range;
            mem_stat->read_ranges_tail = new_range;
        }

        mem_stat->read_cnt++;
        mem_stat->read_bytes += access_size;
    } else if (rw == 2) {
        page_range_t* new_range = new page_range_t();
        new_range->start_page = start_page;
        new_range->end_page = end_page;
        new_range->io_idx = VFD_ACCESS_IDX;
        new_range->next = nullptr;

        if (mem_stat->write_ranges == nullptr) {
            mem_stat->write_ranges = new_range;
            mem_stat->write_ranges_tail = new_range;
        } else {
            mem_stat->write_ranges_tail->next = new_range;
            mem_stat->write_ranges_tail = new_range;
        }

        mem_stat->write_cnt++;
        mem_stat->write_bytes += access_size;
    }
}


void update_mem_type_stat(int rw, size_t start_page, 
  size_t end_page, size_t access_size, H5FD_mem_t type, vfd_file_tkr_info_t * info)
{
  
  switch(type) {
    case H5FD_MEM_DRAW:
      if (info->h5_draw == nullptr) {
        info->h5_draw = new h5_mem_stat_t();
        info->h5_draw->mem_type = "H5FD_MEM_DRAW";
      }
      update_mem_type_stat_helper(rw, start_page, end_page, access_size, info->h5_draw);
      
      break;
    case H5FD_MEM_OHDR:
      if (info->h5_ohdr == nullptr) {
        info->h5_ohdr = new h5_mem_stat_t();
        info->h5_ohdr->mem_type = "H5FD_MEM_OHDR";
      }
      update_mem_type_stat_helper(rw, start_page, end_page, access_size, info->h5_ohdr);
      break;
    case H5FD_MEM_SUPER:
      if (info->h5_super == nullptr) {
          info->h5_super = new h5_mem_stat_t();
          info->h5_super->mem_type = "H5FD_MEM_SUPER";
      }
      update_mem_type_stat_helper(rw, start_page, end_page, access_size, info->h5_super);
      break;
    case H5FD_MEM_BTREE:
      if (info->h5_btree == nullptr) {
          info->h5_btree = new h5_mem_stat_t();
          info->h5_btree->mem_type = "H5FD_MEM_BTREE";
      }
      update_mem_type_stat_helper(rw, start_page, end_page, access_size, info->h5_btree);
      break;
    case H5FD_MEM_LHEAP:
      if (info->h5_lheap == nullptr) {
          info->h5_lheap = new h5_mem_stat_t();
          info->h5_lheap->mem_type = "H5FD_MEM_LHEAP";
      }
      update_mem_type_stat_helper(rw, start_page, end_page, access_size, info->h5_lheap);
      break;
    case H5FD_MEM_GHEAP:
      break;
    case H5FD_MEM_NTYPES:
      break;
    case H5FD_MEM_NOLIST:
      break;
    case H5FD_MEM_DEFAULT:
      break;
    default:
      break;
  }
}




void read_write_info_update(std::string func_name, char * file_name, hid_t fapl_id, H5FD_t *_file,
  H5FD_mem_t type, hid_t dxpl_id, haddr_t addr,
  size_t size, size_t page_size, unsigned long t_start, unsigned long t_end)
{

#ifdef DEBUG_LOG
  printf("read_write_info_update() func_name = %s\n", func_name.c_str());
  // std::cout << "read_write_info_update() file_name = " << file_name << std::endl;
#endif

  H5FD_tracker_vfd_t *file = (H5FD_tracker_vfd_t *)_file;
  vfd_file_tkr_info_t * info = (vfd_file_tkr_info_t *)file->vfd_file_info;


  if (info->file_name == nullptr){
    info->file_name = file_name;
  }


  if (!info->io_bytes || info->io_bytes == 0){
    info->io_bytes = size;
  } else {
    info->io_bytes += size;
  }

  if(func_name == read_func){
    TOTAL_VFD_READ += size;
    info->file_read_cnt++;
    update_mem_type_stat(1, addr/page_size, (addr+size-1)/page_size, size, type, info);
    
  } 
  if(func_name == write_func){
    TOTAL_VFD_WRITE += size;
    info->file_write_cnt++;
    update_mem_type_stat(2, addr/page_size, (addr+size-1)/page_size, size, type, info);
  }

  VFD_ACCESS_IDX+=1;

  TOTAL_TKR_VFD_OVERHEAD+=(get_time_usec() - t_end);

#ifdef DEBUG_LOG
  printf("read_write_info_update() done: info->file_name = %s\n", info->file_name);
  // std::cout << "read_write_info_update() done: info->file_name = " << info->file_name << std::endl;
#endif

#ifdef VFD_LOGGING
  // print_read_write_info(func_name, file_name, fapl_id, _file,
  //   type, dxpl_id, addr, size, page_size, t_start, t_end);
#endif

}

void open_close_info_update(const char* func_name, H5FD_tracker_vfd_t *file, size_t eof, int flags, 
  unsigned long t_start, unsigned long t_end)
{


  if (!TOTAL_TKR_VFD_OVERHEAD)
    TOTAL_TKR_VFD_OVERHEAD = 0;

  // H5FD_tracker_vfd_t *file = (H5FD_tracker_vfd_t *)_file;
  vfd_file_tkr_info_t * info = (vfd_file_tkr_info_t *)file->vfd_file_info;

#ifdef DEBUG_LOG
  printf("open_close_info_update() func_name = %s file_name = %s\n", func_name, info->file_name);
  std::cout << "open_close_info_update() func_name = " << func_name << " file_name = " << info->file_name << std::endl;
#endif

  if (!info->intent)
  {
    std::string file_intent = getFileIntentFlagsStr(flags);
    size_t intent_length = file_intent.length();
    info->intent = (char*)malloc((intent_length + 1) * sizeof(char));
    if (info->intent)
    {
      strncpy(info->intent, file_intent.c_str(), intent_length);
      info->intent[intent_length] = '\0';
    }
    // info->intent = file_intent.c_str() ? strdup(file_intent.c_str()) : nullptr;
  }
  else if (info->intent[0] == '\0')
  {
    std::string file_intent = getFileIntentFlagsStr(flags);
    size_t intent_length = file_intent.length();
    if (intent_length < MAX_FILE_INTENT_LENGTH) // Specify the maximum intent length
    {
      strncpy(info->intent, file_intent.c_str(), intent_length);
      info->intent[intent_length] = '\0';
    }
  }

  if (!info->file_size || info->file_size <= 0)
    info->file_size = eof;

  if (!info->adaptor_page_size)
    info->adaptor_page_size = file->page_size;

  TOTAL_TKR_VFD_OVERHEAD += (get_time_usec() - t_end);

#ifdef DEBUG_LOG
  printf("open_close_info_update() done: info->file_name = %s\n", info->file_name);
  std::cout << "open_close_info_update() done: info->file_name = " << info->file_name << std::endl;
#endif

#ifdef VFD_LOGGING
  // print_open_close_info(func_name, file, file->name, eof, flags, t_start, t_end);
#endif
}


/* candice added, print/record info H5FD__tracker_vfd_open from */
void print_read_write_info(const char* func_name, char * file_name, hid_t fapl_id, void * obj,
  H5FD_mem_t type, hid_t dxpl_id, haddr_t addr,
  size_t size, size_t page_size, unsigned long t_start, unsigned long t_end){

  size_t         start_page_index; /* First page index of tranfer buffer */
  size_t         end_page_index; /* End page index of tranfer buffer */
  size_t         num_pages; /* Number of pages of transfer buffer */
  haddr_t        addr_end = addr + size - 1;
  
  // VFD_ADDR = addr;
  start_page_index = addr/page_size;
  end_page_index = addr_end/page_size;
  num_pages = end_page_index - start_page_index + 1;

  START_ADDR = addr;
  END_ADDR = addr+size;
  ACC_SIZE = size;

  // printf("{\"tracker_vfd_vfd\": ");
  // printf("{tracker_vfd_vfd: ");

  // Note:
  H5FD_t *_file = (H5FD_t *) obj;
  // const H5FD_class_t *f_cls = (H5FD_class_t *) _file->cls;
  // printf("\"f_cls->name\": \"%s\", ", f_cls->name); // "f_cls->fc_degree": "3",
  // printf("\"f_cls->fc_degree\": \"%d\", ", f_cls->fc_degree); //  "f_cls->name": "tracker_vfd", 
  // printf("\"f_cls->value\": \"%p\", ", f_cls->value);
  // printf("\"f_cls->dxpl_size\": \"%d\", ", f_cls->dxpl_size);
  // printf("\"f_cls->fapl_size\": \"%d\", ", f_cls->fapl_size);
  

  printf("{\"func_name\": %s, ", func_name);
  printf("\"io_access_idx\": %ld, ", VFD_ACCESS_IDX);
  printf("\"time(us)\": %ld, ", t_end);
  printf("\"file_no\": %ld, ", _file->fileno); // matches dset_name ?

  // unsigned hash_id = KernighanHash(buf);
  // printf("\"obj(decode)\": \"%p\", ", H5Pdecode(obj));
  // printf("\"dxpl_id\": \"%p\", ", dxpl_id);
  // printf("\"hash_id\": %ld, ", KernighanHash(buf));

  
  printf("\"addr\": [%ld, %ld], ", addr, (addr+size));
  printf("\"access_size\": %ld, ", size);

  // not used
  printf("\"file_pages\": [%ld, %ld], ", start_page_index,end_page_index);
  printf("\"mem_type\": \"%s\", ", get_mem_type(type).c_str());



  printf("\"TOTAL_VFD_READ\": %ld, ", TOTAL_VFD_READ);
  printf("\"TOTAL_VFD_WRITE\": %ld, ", TOTAL_VFD_WRITE);

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

  hsize_t threshold;
  hsize_t alignment;
  H5Pget_alignment(fapl_id, &threshold, &alignment);
  void * buf_ptr_ptr;
  size_t buf_len_ptr;
  H5Pget_file_image(fapl_id, &buf_ptr_ptr, &buf_len_ptr);

  size_t buf_size;
  unsigned min_meta_perc;
  unsigned min_raw_perc;
  if(H5Pget_page_buffer_size(fapl_id, &buf_size, &min_meta_perc, &min_raw_perc) > 0){
      printf("\"H5Pget_page_buffer_size-buf_size\": %ld, ", buf_size);
      printf("\"H5Pget_page_buffer_size-min_meta_perc\": %d, ", min_meta_perc); // TODO: ?
      printf("\"H5Pget_page_buffer_size-min_raw_perc\": %d, ", min_raw_perc);
  }

  printf("\"file_name\": \"%s\", ", strdup(file_name));
  printf("\"vfd_obj\": %p, ", obj);

  printf("}\n");


  // printf("{tracker_vfd_vfd: ");
  // printf("{func_name: %s, ", func_name);
  // printf("obj: %p, ", obj);
  // printf("obj_addr: %p, ", (void *) &obj);
  // printf("dxpl_id: %p, ", dxpl_id);
  // printf("buf: %p, ", buf);
  // printf("buf_addr: %p, ", (void*)&buf);

  // printf("access_size: %ld, ", size);
  // printf("time(us): %ld, ", t_end);
  // printf("file_name: %p, ", file_name);
  
  // printf("start_address: %ld, ", addr);
  // printf("end_address: %ld, ", addr_end);
  // printf("start_page: %ld, ", start_page_index);
  // printf("end_page: %ld, ", end_page_index);
  // printf("num_pages: %d, ", num_pages);
  // printf("mem_type: %s", get_mem_type(type));
  // printf("}}\n");

  /* record and print end */
  
}


void print_open_close_info(const char* func_name, void * obj, const char * file_name, 
  size_t eof, int flags, unsigned long t_start, unsigned long t_end)
{
  H5FD_t *_file = (H5FD_t *) obj;

  // printf("{\"tracker_vfd_vfd\": ");
  // printf("{tracker_vfd_vfd: ");


  printf("{\"func_name\": \"%s\", ", func_name);
  printf("\"time(us)\": \"%ld\", ", t_end);
  // printf("start_time(us): %ld, ", t_start);
  // printf("start_end(us): %ld, ", t_end);
  // printf("start_elapsed(us): %ld, ", (t_end - t_start));
  // printf("\"obj\": \"%p\", ", obj);
  // printf("\"obj_addr\": \"%p\", ", (void *) &obj);
  
  printf("\"file_no\": %ld, ", _file->fileno); // matches dset_name
  printf("\"file_size\": %ld, ", eof);
  printf("\"file_name\": \"%s\", ", file_name);
  printf("}\n");

  // printf("{tracker_vfd_vfd: ");
  // printf("{func_name: %s, ", func_name);
  // // printf("start_time(us): %ld, ", t_start);
  // // printf("start_end(us): %ld, ", t_end);
  // // printf("start_elapsed(us): %ld, ", (t_end - t_start));
  // printf("obj: %p, ", obj);
  // printf("obj_addr: %p, ", (void *) &obj);
  // printf("time(us): %ld, ", t_end);
  // printf("file_name: %s, ", file_name);
  // printf("file_name_hex: %p", file_name);
  // printf("}}\n");

  // initialize & reset
  TOTAL_VFD_READ = 0;
  TOTAL_VFD_WRITE = 0;
  // VFD_ACCESS_IDX = 0; 

}

void print_H5Pset_fapl_info(const char* func_name, hbool_t logStat, size_t page_size)
{
  // printf("{\"tracker_vfd_vfd\": ");
  // printf("{tracker_vfd_vfd: ");
  
  printf("{\"TASK_ID\": \"%d\", ", TASK_ID);
  printf("{\"func_name\": \"%s\", ", func_name);
  printf("\"time(us)\": \"%ld\", ", get_time_usec());
  printf("\"logStat\": \"%s\", ", logStat ? "true" : "false");  
  printf("\"page_size\": \"%ld\", ", page_size);
  // printf("}")
  printf("}\n");
  TASK_ID++;

}


vfd_tkr_helper_t * vfd_tkr_helper_init( char* file_name, size_t page_size, hbool_t logStat)
{
   
    
    vfd_tkr_helper_t* new_helper = (vfd_tkr_helper_t *)calloc(1, sizeof(tkr_helper_t));

    if(logStat) {//write to file
        if(!file_name){
            printf("vfd_tkr_helper_init() failed, vfd-tracker file path is not set.\n");
            return nullptr;
        }
    }

    new_helper->tkr_file_path = strdup(file_name);
    new_helper->logStat = logStat;
    new_helper->pid = getpid();
    new_helper->tid = pthread_self(); // TODO: check if this is correct

    // Update file path with PID
    int prefix_len = snprintf(nullptr, 0, "%d_%s", new_helper->pid, file_name);
    new_helper->tkr_file_path = (char*)malloc(prefix_len + 1);
    snprintf(new_helper->tkr_file_path, prefix_len + 1, "%d_%s", new_helper->pid, file_name);

    printf("vfd new_helper tkr_file_path: %s\n", new_helper->tkr_file_path);

    /* VFD vars start */
    new_helper->vfd_opened_files = nullptr;
    new_helper->vfd_opened_files_cnt = 0;
    new_helper->tracker_vfd_page_size = page_size;
    /* VFD vars end */

    // Get the user's login name
    if (getlogin_r(new_helper->user_name, sizeof(new_helper->user_name)) != 0) {
        printf("Failed to get user name.\n");
    }

    return new_helper;
}


void vfd_file_info_free(vfd_file_tkr_info_t* info)
{
#ifdef H5_HAVE_PARALLEL
    // TODO: VFD not support parallel yet.
#endif /* H5_HAVE_PARALLEL */

    // if(info->file_name)
    //   free((void*)(info->file_name));

    // if(info->task_name)
    //   free((void*)(info->task_name));

    free(info);
}



vfd_file_tkr_info_t* new_vfd_file_info(const char* fname, unsigned long file_no)
{
    vfd_file_tkr_info_t *info;

    info = (vfd_file_tkr_info_t *)calloc(1, sizeof(vfd_file_tkr_info_t));

    info->file_name = fname ? strdup(fname) : nullptr;
    // info->file_name = malloc(sizeof(char) * (strlen(fname) + 1));
    // strcpy(info->file_name, fname);
    info->file_no = file_no;

    // dlLockAcquire(&myLock);
    info->sorder_id =++FILE_SORDER;
    // dlLockRelease(&myLock);

    info->open_time = get_time_usec();
    info->file_read_cnt = 0;
    info->file_write_cnt = 0;

    info->h5_draw = nullptr;
    info->h5_ohdr = nullptr;
    info->h5_super = nullptr;
    info->h5_btree = nullptr;
    info->h5_lheap = nullptr;

    // Usage example:
    std::string task_name = getTaskName();
    if (!task_name.empty()) {
        info->task_name = strdup(task_name.c_str());
        printf("Current task is: %s\n", task_name.c_str());
    } else {
        printf("Failed to get current task.\n");
    }

    // info->task_name = curr_task ? strdup(curr_task) : nullptr;


    // std::string cmdline;
    // int task_pid = getpid();
    // // int task_tid = new_helper->tid;
    // // Get the command line associated with the PID
    // if (getCommandLineByPID(task_pid, cmdline)) {
    // } else {
    // }

    // int pipeHandle;

    // if (extractPipeHandle(cmdline, pipeHandle)) {
    //     // Now, you can use the 'pipeHandle' to interact with the file or perform other actions.
    // } else {
    // }

    // // You might want to open the file descriptor, depending on your use case
    // // For example, open it for reading:
    // int fd = pipeHandle; // Assigning the descriptor

    // // Read from the file descriptor (assuming it's open for reading)
    // char buffer[1024];
    // ssize_t bytesRead = read(fd, buffer, sizeof(buffer));

    // if (bytesRead > 0) {
    //     // Process the data as needed
    // } else if (bytesRead == 0) {
    // } else {
    //     perror("Error reading from descriptor");
    // }
    // // Close the file descriptor when done (if it was opened)
    // close(fd);

    return info;
}



vfd_file_tkr_info_t* add_vfd_file_node(vfd_tkr_helper_t * helper, const char* file_name, void * obj)
{
  unsigned long start = get_time_usec();
  vfd_file_tkr_info_t* cur;
  H5FD_t *_file = (H5FD_t *) obj;
  unsigned long file_no = _file->fileno;

  assert(helper);

  if(!helper->vfd_opened_files){ //empty linked list, no opened file.
    assert(helper->vfd_opened_files_cnt == 0);
    // return 0;
  }
      
  if(!helper->vfd_opened_files) //empty linked list, no opened file.
    assert(helper->vfd_opened_files_cnt == 0);
  

  // Search for file in list of currently opened ones
  cur = helper->vfd_opened_files;
  while (cur) {
      // assert(cur->file_no);

      if (cur->file_no == file_no)
        break;

      cur = cur->next;
  }

  // Allocate and initialize new file node
  cur = new_vfd_file_info(file_name, file_no);

  // Add to linked list
  cur->next = helper->vfd_opened_files;
  helper->vfd_opened_files = cur;
  helper->vfd_opened_files_cnt++;

  // Increment refcount on file node
  cur->ref_cnt++;

  // FILE_LL_TOTAL_TIME += (get_time_usec() - start);
  return cur;
}



//need a dumy node to make it simpler
int rm_vfd_file_node(vfd_tkr_helper_t* helper, H5FD_t *_file)
{


  unsigned long start = get_time_usec();
  vfd_file_tkr_info_t* cur;
  vfd_file_tkr_info_t* last;
  unsigned long file_no = _file->fileno;

  assert(helper);
  assert(helper->vfd_opened_files);
  assert(helper->vfd_opened_files_cnt);
  assert(file_no);

  cur = helper->vfd_opened_files;
  last = cur;

  while (cur) {
    assert(cur);
    assert(cur->ref_cnt);

    // Node found
    if (cur->file_no == file_no) {
        // Decrement file node's refcount
        cur->ref_cnt--;

        // If refcount == 0, remove file node & maybe print file stats
        if (cur->ref_cnt == 0) {

            // Unlink from list of opened files
            if (cur == helper->vfd_opened_files) { // first node is the target
                helper->vfd_opened_files = cur->next;
                vfd_file_info_free(cur); // Free file info here
                cur = helper->vfd_opened_files; // Move to the next node
            } else {
                last->next = cur->next;
                vfd_file_info_free(cur); // Free file info here
                cur = last->next; // Move to the next node
            }

            // Update connector info
            helper->vfd_opened_files_cnt--;
            if (helper->vfd_opened_files_cnt == 0)
                assert(helper->vfd_opened_files == nullptr);
        } else {
            last = cur;
            cur = cur->next;
        }
    } else {
        last = cur;
        cur = cur->next;
    }
  }

  TOTAL_TKR_VFD_OVERHEAD += (get_time_usec() - start);

  return helper->vfd_opened_files_cnt;
}

void vfd_tkr_helper_teardown(vfd_tkr_helper_t* helper){
  printf("vfd_tkr_helper_teardown(): TKR_HELPER_VFD\n");
  // frea down causes double free error in single process mode

  if(helper){// not null

      // if(helper->logStat){//no file
      //   fflush(helper->tkr_file_handle);
      //   fclose(helper->tkr_file_handle);
      // }

      // if(helper->tkr_file_path)
      //     free(helper->tkr_file_path);

      free(helper);
  }

}
