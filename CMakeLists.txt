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

# FREERTOS: include FreeRTOS Kernel libraries
include(libs/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)

# 5. Define the executable and its source files
# Point this to your actual main file location
add_executable(hub_master 
    projects/hub-master/main.cpp
)

# 6. Add include directories (where your headers live)
target_include_directories(hub_master PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}/projects/hub-master
    ${CMAKE_CURRENT_LIST_DIR}/shared
)

# 7. Link the actual libraries
target_link_libraries(hub_master 
    pico_stdlib 
    FreeRTOS-Kernel-Heap4
)

# 8. Create ELF, UF2, etc.
pico_add_extra_outputs(hub_master)




# 3. Add your sub-projects
# This keeps the Hub and Satellite builds clean and separate
#add_subdirectory(projects/hub-master)
# add_subdirectory(projects/satellite-node) # Enable when ready