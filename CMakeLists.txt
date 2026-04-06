# The following CMake or environment variables should be defined:
# PICO_SDK_PATH = path/to/sdk
# FREERTOS_KERNEL_PATH = path/to/FreeRTOS-Kernel
cmake_minimum_required(VERSION 3.14)

# Pull in SDK (must be before project)
include(libs/pico-sdk/external/pico_sdk_import.cmake)
project(freertos-sample C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


# Initialize the SDK
pico_sdk_init()

# 1. Explicitly set the absolute path to the FreeRTOS Kernel
set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libs/FreeRTOS-Kernel")

# Provide FreeRTOS with the path to FreeRTOSConfig.h
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE 
    ${CMAKE_CURRENT_LIST_DIR}/projects/hub-master
    ${CMAKE_CURRENT_LIST_DIR}/shared
)

# Load the shared library target
add_subdirectory(shared)

# FREERTOS: include FreeRTOS Kernel libraries
include(libs/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)

# 3. Add your sub-projects
# This keeps the Hub and Satellite builds clean and separate
add_subdirectory(projects/hub-master)
# add_subdirectory(projects/satellite-node) # Enable when ready