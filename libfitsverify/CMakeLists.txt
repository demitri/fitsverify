add_library(fitsverify
    src/fv_api.c
    src/fv_hints.c
    src/fvrf_misc.c
    src/fvrf_key.c
    src/fvrf_file.c
    src/fvrf_data.c
    src/fvrf_head.c
)

target_include_directories(fitsverify
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CFITSIO_INCLUDE_DIRS}
)


if(CFITSIO_LIBRARY_DIRS)
    target_link_directories(fitsverify PUBLIC ${CFITSIO_LIBRARY_DIRS})
endif()
target_link_libraries(fitsverify PUBLIC ${CFITSIO_LIBRARIES})

# Also link math library on Unix
if(UNIX)
    target_link_libraries(fitsverify PUBLIC m)
endif()
