# CMakeLists.txt in src/vol directory

add_library(h5tracker SHARED tracker_vol_new.c)

target_compile_options(h5tracker PRIVATE -g -O0 -DDEBUG -gdwarf-4 -gdwarf-3 -gdwarf-2 -fPIC -I${HDF5_DIR}/include -w -DTRACKER_SCHEMA -Wall)
target_link_libraries(h5tracker PRIVATE ${DYNLDFLAGS})

install(TARGETS h5tracker LIBRARY DESTINATION lib)
