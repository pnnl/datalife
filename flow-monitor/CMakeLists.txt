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


#****************************************************************************
# 
#****************************************************************************

set(LIB_PATH "${CMAKE_BINARY_DIR}/src/libmonitor.so")

# tallent:
#   CMAKE_SOURCE_DIR: source dir of any top level (outer) 'project' command
#   PROJECT_SOURCE_DIR: source dir of most recent 'project' command
SET(MONITOR_INC_DIR ${PROJECT_SOURCE_DIR}/inc)

add_subdirectory(src)
