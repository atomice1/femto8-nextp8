cmake_minimum_required(VERSION 3.10)
project(p8_editor_tests)

# Find CMocka
find_package(cmocka REQUIRED)

# Find SDL2
find_package(SDL2 REQUIRED)

# Add the editor subdirectory
add_subdirectory(editor)

# Add the http subdirectory
add_subdirectory(http)

# Add the emu subdirectory
add_subdirectory(emu)

# Add the dialog subdirectory
add_subdirectory(dialog)

# Run tests
enable_testing()
add_test(NAME p8_editor_code_tests COMMAND p8_editor_code_test)
add_test(NAME p8_http_tests COMMAND p8_http_test)
add_test(NAME p8_emu_tests COMMAND p8_emu_tests)
add_test(NAME p8_dialog_tests COMMAND p8_dialog_tests)
