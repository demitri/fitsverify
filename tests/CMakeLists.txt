# Test FITS file generator
add_executable(gen_test_fits gen_test_fits.c)
target_include_directories(gen_test_fits PRIVATE ${CFITSIO_INCLUDE_DIRS})
if(CFITSIO_LIBRARY_DIRS)
    target_link_directories(gen_test_fits PRIVATE ${CFITSIO_LIBRARY_DIRS})
endif()
target_link_libraries(gen_test_fits ${CFITSIO_LIBRARIES})
if(UNIX)
    target_link_libraries(gen_test_fits m)
endif()

# Library API test
add_executable(test_library_api test_library_api.c)
target_link_libraries(test_library_api fitsverify)
target_include_directories(test_library_api PRIVATE ${CFITSIO_INCLUDE_DIRS})

# Abort test
add_executable(test_abort test_abort.c)
target_link_libraries(test_abort fitsverify)
target_include_directories(test_abort PRIVATE ${CFITSIO_INCLUDE_DIRS})

# Copy regression test script to build dir
configure_file(test_regression.sh ${CMAKE_CURRENT_BINARY_DIR}/test_regression.sh COPYONLY)
