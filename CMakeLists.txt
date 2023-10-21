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

option(DO_MAKE_FlowMonitor "Enable build of DataLife-FlowMonitor" ON)
option(DO_MAKE_FlowAnalysis "Enable build of DataLife-FlowAnalysis" ON)

option(BUILD_TESTS "Build test programs" OFF)


set(DATALIFE_ROOT ${CMAKE_BINARY_DIR})

if(DO_MAKE_FlowMonitor)
  add_subdirectory(flow-monitor)
endif(DO_MAKE_FlowMonitor)

if(DO_MAKE_FlowAnalysis)
  add_subdirectory(flow-analysis)
endif(DO_MAKE_FlowAnalysis)

message(STATUS "DATALIFE_ROOT=${DATALIFE_ROOT}")
#execute_process(COMMAND echo ${DATALIFE_ROOT})
