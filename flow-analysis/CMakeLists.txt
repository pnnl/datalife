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
project(DataLife-FlowAnalysis)

find_package(Python COMPONENTS Interpreter REQUIRED)

# detect virtualenv and set Pip args accordingly
if(DEFINED ENV{VIRTUAL_ENV} OR DEFINED ENV{CONDA_PREFIX})
    set(_pip_args)
else()
    set(_pip_args "--user")
endif()

if (NOT DEFINED DATALIFE_ROOT)
    set(DATALIFE_ROOT ${CMAKE_BINARY_DIR})
    set(_pypkg ${DATALIFE_ROOT}/../)
    set(_pypkg_libexec ${DATALIFE_ROOT}/libexec)
else()
    set(_pypkg ${DATALIFE_ROOT}/../flow-analysis)
    set(_pypkg_libexec ${DATALIFE_ROOT}/flow-analysis/libexec)
endif()

message(STATUS "Python_EXECUTABLE: ${Python_EXECUTABLE}")
message(STATUS "${Python_EXECUTABLE} -m pip install ${_pip_args} ${_pypkg} --target ${_pypkg_libexec}")
execute_process(COMMAND ${Python_EXECUTABLE} -m pip install ${_pip_args} ${_pypkg} --target ${_pypkg_libexec})
add_custom_target(flow-analysis-bin)
add_custom_command(
    TARGET flow-analysis-bin PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${_pypkg}/datalife-analyze
    ${CMAKE_CURRENT_BINARY_DIR}/bin/datalife-analyze)
