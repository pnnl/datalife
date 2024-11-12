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
project(DataLife-FlowAnalysis)

#****************************************************************************
# Environment
#****************************************************************************

if (NOT DEFINED DATALIFE_ROOT)
    set(DATALIFE_ROOT ${CMAKE_BINARY_DIR})
    set(_pypkg ${DATALIFE_ROOT}/../)
    set(_pypkg_libexec ${DATALIFE_ROOT}/libexec/flow-analysis)
else()
    set(_pypkg ${DATALIFE_ROOT}/../flow-analysis)
    set(_pypkg_libexec ${DATALIFE_ROOT}/flow-analysis/libexec/flow-analysis)
endif()


#****************************************************************************
# Obtain Dependences
#****************************************************************************

find_package(Python COMPONENTS Interpreter REQUIRED)
message(STATUS "Python_EXECUTABLE: ${Python_EXECUTABLE}")

# detect virtualenv and set Pip args accordingly
if(DEFINED ENV{VIRTUAL_ENV} OR DEFINED ENV{CONDA_PREFIX})
    set(_pip_args)
else()
    set(_pip_args)
    # raise DistutilsOptionError("can't combine user with prefix, "
    #set(_pip_args "--user")
endif()


message(STATUS "${Python_EXECUTABLE} -m pip install ${_pip_args} ${_pypkg} --target ${_pypkg_libexec}")
execute_process(COMMAND ${Python_EXECUTABLE} -m pip install ${_pip_args} ${_pypkg} --target ${_pypkg_libexec})


#****************************************************************************
# 
#****************************************************************************

install(FILES datalife-analyze DESTINATION bin PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(DIRECTORY ${_pypkg_libexec} DESTINATION libexec USE_SOURCE_PERMISSIONS)
