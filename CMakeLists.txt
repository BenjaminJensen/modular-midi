cmake_minimum_required(VERSION 3.14)

# 1. Pull in SDK and Initialize (must be at the top)
include(libs/pico-sdk/external/pico_sdk_import.cmake)
project(modular-midi C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# --- FreeRTOS SETUP ---
set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/libs/FreeRTOS-Kernel")

# Use the specific RP2350 port for the M33 cores
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake)

# --- LVGL SETUP ---
set(LVGL_DIR "${CMAKE_CURRENT_LIST_DIR}/libs/lvgl")

# Use CURRENT_LIST_DIR to ensure it points to the root of your repo
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/projects/hub-master/lv_conf.h")
add_subdirectory(${LVGL_DIR} lvgl)

# LVGL requires FreeRTOS include paths and your FreeRTOSConfig.h to build its OSAL.
# We apply these to lvgl, and its optional example/demo targets.
# foreach(LV_TARGET lvgl lvgl_examples lvgl_demos)
#     if(TARGET ${LV_TARGET})
#         target_include_directories(${LV_TARGET} PRIVATE 
#             ${CMAKE_CURRENT_SOURCE_DIR}/projects/hub-master
#             ${CMAKE_CURRENT_SOURCE_DIR}/shared
#         )
#         target_link_libraries(${LV_TARGET} PRIVATE FreeRTOS-Kernel-Heap4)
#     endif()
# endforeach()

# 5. Define Executable
add_executable(hub_master 
    projects/hub-master/main.cpp
)

# 6. Global SDK Feature Toggles
pico_enable_stdio_usb(hub_master 0)
pico_enable_stdio_uart(hub_master 0)

# 7. Include Paths
target_include_directories(hub_master PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/projects/hub-master
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
)

# 8. Linkage
target_link_libraries(hub_master 
    pico_stdlib 
    pico_stdio_rtt
    FreeRTOS-Kernel-Heap4
    lvgl
)

# 9. Output Generation (UF2 for flashing, ELF for Debugging)
pico_add_extra_outputs(hub_master)