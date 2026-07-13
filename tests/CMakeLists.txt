# tests/CMakeLists.txt
#
# Independent, host-only CMake project for logic that doesn't touch hardware.
# Deliberately separate from the root CMakeLists.txt, which unconditionally
# pulls in pico-sdk (ARM cross-compile) before project() is even called.

cmake_minimum_required(VERSION 3.14)
project(modular-midi-tests CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(button_tests
    shared/button_test.cpp
)

target_include_directories(button_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/libs/doctest
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME button_tests COMMAND button_tests)
