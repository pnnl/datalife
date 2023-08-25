cmake_minimum_required (VERSION 2.8)
project(Monitor)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage -Wall -pedantic -pthread -g -Wl,--no-as-needed -ldl -fPIC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage -ftls-model=global-dynamic -fPIC ")

option(INPUT_DIR "Default input directory" ON)
if(INPUT_DIR)
    set(INPUT_DIR_PATH "${CMAKE_BINARY_DIR}/inputs")
    add_definitions(-DINPUTS_DIR="${CMAKE_BINARY_DIR}/inputs")
else()
    set(INPUT_DIR_PATH ".")
endif()

set(LIB_PATH "${CMAKE_BINARY_DIR}/src/libmonitor.so")
SET(MONITOR_INC_DIR ${CMAKE_SOURCE_DIR}/inc)
add_subdirectory(src)
