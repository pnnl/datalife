# -*-Mode: cmake;-*-

#*BeginPNNLCopyright*********************************************************
#
# $HeadURL$
# $Id$
#
#***********************************************************EndPNNLCopyright*

#****************************************************************************
# 
#****************************************************************************

cmake_minimum_required(VERSION 2.8...3.27)
project(DataLife-FlowMonitor)

#****************************************************************************
# Options
#****************************************************************************

# Compiler
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -pthread -g -Wl,--no-as-needed -ldl -fPIC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftls-model=global-dynamic -fPIC ")

# Input dir
option(INPUT_DIR "Default input directory" ON)
if(INPUT_DIR)
    set(INPUT_DIR_PATH "${CMAKE_BINARY_DIR}/inputs")
    add_definitions(-DINPUTS_DIR="${CMAKE_BINARY_DIR}/inputs")
else()
    set(INPUT_DIR_PATH ".")
endif()

# Compiling Mode
# Set a default build type if none was specified
set(default_build_type "Release")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("IN DEBUG MODE")
    add_compile_options(-g -O0)
    add_compile_definitions(LIBDEBUG)
else()
    message("IN RELEASE MODE")
    add_compile_options(-g -O3)
endif()

# Datalife Options
option(GATHERSTAT "Gather statistics" OFF)
if(GATHERSTAT)
    add_compile_definitions(GATHERSTAT)
endif()
option(BLK_IDX "Block Index" OFF)
if(BLK_IDX)
    add_compile_definitions(BLK_IDX)
endif()
option(INT_DOT "Intercept .dot files" OFF)
if(INT_DOT)
    add_compile_definitions(INT_DOT)
endif()

option(TIMER_JSON "Log timer data in JSON format (default to stdout)" OFF)
if(TIMER_JSON)
    add_compile_definitions(TIMER_JSON)
endif()

#****************************************************************************
# 
#****************************************************************************

set(LIB_PATH "${CMAKE_BINARY_DIR}/src/libmonitor.so")

# tallent:
#   CMAKE_SOURCE_DIR: source dir of any top level (outer) 'project' command
#   PROJECT_SOURCE_DIR: source dir of most recent 'project' command
SET(MONITOR_INC_DIR ${PROJECT_SOURCE_DIR}/inc)

add_subdirectory(src)
