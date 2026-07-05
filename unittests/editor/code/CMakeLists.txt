# Code editor unit tests

add_executable(p8_editor_code_test
    p8_editor_code_test.c
    test_basic.c
    test_edit.c
    test_undo.c
    ../stubs.c
    ${CMAKE_SOURCE_DIR}/../src/p8_editor_code.c
    ${CMAKE_SOURCE_DIR}/../src/p8_editor_syntax.c
    ${CMAKE_SOURCE_DIR}/../src/strtcpy.c
)

# With -Dstatic=, static functions/vars become regular global
set_source_files_properties(${CMAKE_SOURCE_DIR}/../src/p8_editor_code.c PROPERTIES COMPILE_DEFINITIONS -Dstatic=)

target_include_directories(p8_editor_code_test PRIVATE
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/../src/data
    ${CMAKE_SOURCE_DIR}/../src/lua
    ${CMAKE_SOURCE_DIR}/../src/lodepng
    ${CMAKE_SOURCE_DIR}/../src/lexaloffle
    ${SDL2_INCLUDE_DIRS}
)

# Generate extern declarations header
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(GENERATED_HEADER ${CMAKE_CURRENT_BINARY_DIR}/p8_editor_code_priv.h)
set(SOURCES
    ${CMAKE_SOURCE_DIR}/../src/p8_editor_code.c
    ${CMAKE_SOURCE_DIR}/../src/p8_editor_syntax.c
)

add_custom_command(
    OUTPUT ${GENERATED_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../generate_test_header.py ${GENERATED_HEADER} ${SOURCES}
    DEPENDS ${SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/../generate_test_header.py
    COMMENT "Generating p8_editor_code_priv.h with extern declarations"
)

add_custom_target(p8_editor_header DEPENDS ${GENERATED_HEADER})

target_include_directories(p8_editor_code_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

find_package(cmocka REQUIRED)
find_package(SDL2 REQUIRED)

target_link_libraries(p8_editor_code_test PRIVATE cmocka ${SDL2_LIBRARIES})

add_dependencies(p8_editor_code_test p8_editor_header)
