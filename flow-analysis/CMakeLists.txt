cmake_minimum_required (VERSION 2.8)
project(Analyze)

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

execute_process(COMMAND echo ${Python_EXECUTABLE} -m pip install ${_pip_args} ${_pypkg} --target ${_pypkg_libexec})
execute_process(COMMAND ${Python_EXECUTABLE} -m pip install ${_pip_args} ${_pypkg} --target ${_pypkg_libexec})

