cmake_minimum_required (VERSION 2.8)
project(datalife)

set(DATALIFE_ROOT ${CMAKE_BINARY_DIR})

add_subdirectory(flow-analysis)
add_subdirectory(flow-monitor)

execute_process(COMMAND echo ${DATALIFE_ROOT})
