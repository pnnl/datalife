cmake_minimum_required (VERSION 2.8)
project(Analyze)

find_package(Python COMPONENTS Interpreter REQUIRED)

# detect virtualenv and set Pip args accordingly
if(DEFINED ENV{VIRTUAL_ENV} OR DEFINED ENV{CONDA_PREFIX})
    set(_pip_args)
else()
    set(_pip_args "--user")
endif()

set(_pypkg "..")

execute_process(COMMAND ${Python_EXECUTABLE} -m pip install ${_pip_args} ${_pypkg})

