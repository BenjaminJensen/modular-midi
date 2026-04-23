# The following CMake or environment variables should be defined:
# PICO_SDK_PATH = path/to/sdk
# FREERTOS_KERNEL_PATH = path/to/FreeRTOS-Kernel
cmake_minimum_required(VERSION 3.14)

# Pull in SDK (must be before project)
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libs/pico-sdk")
include(libs/pico-sdk/external/pico_sdk_import.cmake)
project(freertos-sample C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# --- LVGL Configuration ---

# Do not build, demos, exambnles and ThorVG
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Do not build examples")
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Do not build demos")
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "Do not build ThorVG")


# This tells LVGL not to automatically include everything
set(LV_CONF_BUILD_DISABLE_ALL_WIDGETS ON CACHE BOOL "")

# 1. Define the exact location of your real config
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/projects/hub-master/lv_conf.h" CACHE INTERNAL "")

# 2. This is the crucial part: Tell LVGL NOT to search for the file itself
set(LV_DRV_NO_CONF ON CACHE INTERNAL "") 

# 3. Standard LVGL skip-logic definitions
add_definitions(-DLV_CONF_SKIP=0)
add_definitions(-DLV_CONF_INCLUDE_SIMPLE=1)

# Initialize the SDK
pico_sdk_init()

# Explicitly set the absolute path to the FreeRTOS Kernel
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




# Add the LVGL subdirectory
# This makes the 'lvgl' and 'lvgl::lvgl' targets available
add_subdirectory(libs/lvgl)

# Add your sub-projects
# This keeps the Hub and Satellite builds clean and separate
add_subdirectory(projects/hub-master)
# add_subdirectory(projects/satellite-node) # Enable when ready
