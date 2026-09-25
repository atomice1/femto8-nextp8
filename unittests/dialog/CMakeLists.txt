# Dialog unit tests

add_executable(p8_dialog_tests
    p8_dialog_test.c
    test_input.c
    stubs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/p8_emu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/p8_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/strtcpy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/p8_dialog.c
)

# With -Dstatic=, static functions/vars become regular global
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../../src/p8_dialog.c PROPERTIES COMPILE_DEFINITIONS -Dstatic=)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../../src/p8_input.c PROPERTIES COMPILE_DEFINITIONS -Dstatic=)

target_include_directories(p8_dialog_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/data
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lua
    ${SDL2_INCLUDE_DIRS}
)

# Generate extern declarations header
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(GENERATED_HEADER ${CMAKE_CURRENT_BINARY_DIR}/p8_dialog_test_priv.h)
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/p8_dialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/p8_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/p8_emu.c
)

add_custom_command(
    OUTPUT ${GENERATED_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_test_header.py ${GENERATED_HEADER} ${SOURCES}
    DEPENDS ${SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_test_header.py
    COMMENT "Generating p8_dialog_test_priv.h with extern declarations"
)

add_custom_target(p8_dialog_header DEPENDS ${GENERATED_HEADER})

target_include_directories(p8_dialog_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(p8_dialog_tests PRIVATE cmocka ${SDL2_LIBRARIES})

add_dependencies(p8_dialog_tests p8_dialog_header)
