add_executable(fitsverify_cli fitsverify.c)

# Link against the library
target_link_libraries(fitsverify_cli PRIVATE fitsverify)

target_include_directories(fitsverify_cli PRIVATE
    ${CFITSIO_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/libfitsverify/src
)

set_target_properties(fitsverify_cli PROPERTIES OUTPUT_NAME fitsverify)
