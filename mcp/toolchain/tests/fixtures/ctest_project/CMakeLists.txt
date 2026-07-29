# Standalone fixture project for validating server.py's ctest-output
# parsing against real ctest output. Deliberately isolated from the repo's
# own tests/ CMake project and never wired into the root CMakeLists.txt.
#
# ctest reports pass/fail per executable, not per doctest TEST_CASE, so two
# small executables (one always-passing, one always-failing) are needed to
# get a real "N passed / M failed out of T" summary line — mirrors the
# shape of the real tests/CMakeLists.txt (one add_test per executable).

cmake_minimum_required(VERSION 3.14)
project(toolchain-mcp-fixture CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

set(DOCTEST_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/libs/doctest)

add_executable(passing_test passing_test.cpp)
target_include_directories(passing_test PRIVATE ${DOCTEST_INCLUDE_DIR})
add_test(NAME passing_test COMMAND passing_test)

add_executable(failing_test failing_test.cpp)
target_include_directories(failing_test PRIVATE ${DOCTEST_INCLUDE_DIR})
add_test(NAME failing_test COMMAND failing_test)
