# CMakeLists.txt in src/vfd directory

add_library(hdf5_tracker_vfd SHARED H5FD_tracker_vfd.cc)

target_compile_options(hdf5_tracker_vfd PRIVATE -g -O0 -DDEBUG -gdwarf-4 -gdwarf-3 -gdwarf-2 -fPIC -I${HDF5_DIR}/include -w -Wall ${POSIX_FLAGS})
target_link_libraries(hdf5_tracker_vfd PRIVATE ${DYNLDFLAGS})

install(TARGETS hdf5_tracker_vfd LIBRARY DESTINATION lib)
