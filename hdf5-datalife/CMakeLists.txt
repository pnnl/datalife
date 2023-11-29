# CMakeLists.txt

cmake_minimum_required(VERSION 3.12)

project(HDF5_Tracker)

# Find HDF5 with a required version of 1.14
find_package(HDF5 1.14 REQUIRED)
if (HDF5_FOUND)
    include_directories(${HDF5_INCLUDE_DIRS})
endif()

# Option for VOL_TRACKER
option(VOL_TRACKER "Build VOL Tracker" OFF)
if(VOL_TRACKER)
    add_subdirectory(src/vol)
endif()

# Option for VFD_TRACKER
option(VFD_TRACKER "Build VFD Tracker" OFF)
if(VFD_TRACKER)
    add_subdirectory(src/vfd)
endif()