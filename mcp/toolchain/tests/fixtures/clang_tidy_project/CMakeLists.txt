# Standalone fixture project for validating lint_file()/tools/clang-tidy.py
# against real clang-tidy output. Deliberately isolated from the repo's own
# CMake projects and never wired into the root CMakeLists.txt.
#
# CMAKE_EXPORT_COMPILE_COMMANDS is written at configure time, so the test
# only needs to run `cmake -G Ninja ...` here -- no `cmake --build` is
# required to get a usable compile_commands.json. The test also passes
# -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY, since without it CMake's
# own compiler-check step tries to *link* a throwaway test program, which
# fails for this bare-metal ARM target (no libc syscall stubs/linker
# script, unlike the real firmware's project() setup) even though actually
# compiling a translation unit -- all clang-tidy needs -- works fine.
#
# The compiler/target flags are passed on the cmake command line by the
# test itself (mirroring ctest_project's -DCMAKE_CXX_COMPILER=... style),
# not hardcoded here, so this exercises the same arm-none-eabi-g++ +
# -mcpu=cortex-m33 -mthumb shape real firmware translation units compile
# with -- the codepath tools/clang-tidy.py's arm_extra_args() layers its
# --target/--sysroot flags on top of.

cmake_minimum_required(VERSION 3.14)
project(toolchain-mcp-clang-tidy-fixture CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(clang_tidy_fixture OBJECT nullptr_finding.cpp)
