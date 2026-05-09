# shared/CMakeLists.txt 

add_library(shared STATIC "")

# Link the intermediate hal directory
add_subdirectory(hal)
add_subdirectory(services)

# Keep common files and includes here 
target_sources(shared PRIVATE
    freertos_support.cpp
)

target_include_directories(shared PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(shared PUBLIC 
    lvgl
    FreeRTOS-Kernel
)