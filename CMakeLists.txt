# -*-Mode: cmake;-*-

#*BeginPNNLCopyright*********************************************************
#
# $HeadURL$
# $Id: b3a9434b0207745daf99726c43f05688ac1e068f $
#
#***********************************************************EndPNNLCopyright*

#****************************************************************************
# 
#****************************************************************************

cmake_minimum_required(VERSION 2.8...3.27)
project(DataLife VERSION 0.1.0)

set(DATALIFE_ROOT ${CMAKE_BINARY_DIR})

add_subdirectory(flow-analysis)
add_subdirectory(flow-monitor)

message(STATUS "DATALIFE_ROOT=${DATALIFE_ROOT}")
#execute_process(COMMAND echo ${DATALIFE_ROOT})
