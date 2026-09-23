# p8_emu unit tests

# With -Dstatic=, static functions/vars become regular global
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../../src/p8_emu.c PROPERTIES COMPILE_DEFINITIONS -Dstatic=)

add_executable(p8_emu_tests
    p8_emu_test.c
    stubs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/p8_emu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/strtcpy.c
)

target_include_directories(p8_emu_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/data
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(p8_emu_tests PRIVATE cmocka ${SDL2_LIBRARIES})
