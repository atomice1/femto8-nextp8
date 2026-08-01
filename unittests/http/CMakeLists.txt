# HTTP client unit tests

# Main test executable
add_executable(p8_http_test
    p8_http_test.c
    test_http.c
    stubs.c
    ${CMAKE_SOURCE_DIR}/../src/p8_http.c
    ${CMAKE_SOURCE_DIR}/../src/strtcpy.c
)

# With -Dstatic=, static functions/vars become regular global
set_source_files_properties(${CMAKE_SOURCE_DIR}/../src/p8_http.c PROPERTIES COMPILE_DEFINITIONS -Dstatic=)

target_include_directories(p8_http_test PRIVATE
    ${CMAKE_SOURCE_DIR}/../src
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(p8_http_test PRIVATE cmocka ${SDL2_LIBRARIES})
