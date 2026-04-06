# The following CMake or environment variables should be defined:
# PICO_SDK_PATH = path/to/sdk
# FREERTOS_KERNEL_PATH = path/to/FreeRTOS-Kernel
cmake_minimum_required(VERSION 3.14)

# Pull in SDK (must be before project)
include(libs/pico-sdk/external/pico_sdk_import.cmake)
project(freertos-sample C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(PICO_FREERTOS_LIB_SMP=1)

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

# 1. Enable the RTT driver in the SDK
pico_enable_stdio_rtt(hub_master 1)

# 2. Disable others to prevent resource conflicts
pico_enable_stdio_usb(hub_master 0)
pico_enable_stdio_uart(hub_master 0)

# 7. Link the actual libraries
target_link_libraries(hub_master 
    pico_stdlib 
    pico_stdio_rtt
    FreeRTOS-Kernel-Heap4
)

# 8. Create ELF, UF2, etc.
pico_add_extra_outputs(hub_master)




# 3. Add your sub-projects
# This keeps the Hub and Satellite builds clean and separate
#add_subdirectory(projects/hub-master)
# add_subdirectory(projects/satellite-node) # Enable when ready