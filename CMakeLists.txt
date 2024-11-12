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
project(DataLife VERSION 0.1.0)


#****************************************************************************
# Options
#****************************************************************************

option(ENABLE_FlowMonitor "Enable build of DataLife-FlowMonitor" ON)
option(ENABLE_FlowAnalysis "Enable build of DataLife-FlowAnalysis" ON)
option(ENABLE_TESTS "Build test programs" OFF)

#****************************************************************************
# 
#****************************************************************************

set(DATALIFE_ROOT ${CMAKE_BINARY_DIR})
message(STATUS "DATALIFE_ROOT=${DATALIFE_ROOT}")

if(ENABLE_FlowMonitor)
  add_subdirectory(flow-monitor)
endif(ENABLE_FlowMonitor)

if(ENABLE_FlowAnalysis)
  add_subdirectory(flow-analysis)
endif(ENABLE_FlowAnalysis)


#****************************************************************************

# install datalife-run
#add_executable(datalife-run flow-monitor/datalife-run)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# if(ENABLE_FlowMonitor) endif(ENABLE_FlowMonitor)
configure_file(flow-monitor/datalife-run ${CMAKE_BINARY_DIR}/bin COPYONLY)

#if(ENABLE_FlowAnalysis) endif(ENABLE_FlowAnalysis)
configure_file(flow-analysis/datalife-analyze ${CMAKE_BINARY_DIR}/bin COPYONLY)

