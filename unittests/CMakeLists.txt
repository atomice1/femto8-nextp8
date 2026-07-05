cmake_minimum_required(VERSION 3.10)
project(p8_editor_tests)

# Find CMocka
find_package(cmocka REQUIRED)

# Find SDL2
find_package(SDL2 REQUIRED)

# Add the editor subdirectory
add_subdirectory(editor)

# Run tests
enable_testing()
add_test(NAME p8_editor_tests COMMAND p8_editor_tests)
