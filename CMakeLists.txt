cmake_minimum_required(VERSION 3.14)
project(fitsverify C)

set(CMAKE_C_STANDARD 99)

# Find CFITSIO
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CFITSIO QUIET cfitsio)
endif()

if(NOT CFITSIO_FOUND)
    find_path(CFITSIO_INCLUDE_DIRS fitsio.h
        HINTS ENV CFITSIO_DIR
        PATH_SUFFIXES include)
    find_library(CFITSIO_LIBRARIES cfitsio
        HINTS ENV CFITSIO_DIR
        PATH_SUFFIXES lib)
    if(CFITSIO_INCLUDE_DIRS AND CFITSIO_LIBRARIES)
        set(CFITSIO_FOUND TRUE)
    else()
        message(FATAL_ERROR "CFITSIO not found. Set CFITSIO_DIR or install cfitsio.")
    endif()
endif()

message(STATUS "CFITSIO include: ${CFITSIO_INCLUDE_DIRS}")
message(STATUS "CFITSIO library: ${CFITSIO_LIBRARIES}")

add_subdirectory(libfitsverify)
add_subdirectory(cli)
add_subdirectory(tests)
