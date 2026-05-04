# The following CMake or environment variables should be defined:
# PICO_SDK_PATH = path/to/sdk
# FREERTOS_KERNEL_PATH = path/to/FreeRTOS-Kernel
cmake_minimum_required(VERSION 3.14)


set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
# This is placed top-level as CMkake must have a "project" defined toplevel
# Pull in SDK (must be before project)
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/pico-sdk")
include(src/libs/pico-sdk/external/pico_sdk_import.cmake)

project(modular-midi C CXX ASM)

add_subdirectory(src)