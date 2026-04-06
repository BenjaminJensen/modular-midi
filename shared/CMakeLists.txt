# Create a STATIC library to compile our shared source files
add_library(shared STATIC
    hal/rp2350/st7789.cpp
)

target_include_directories(shared PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/hal/rp2350
)

# Link the underlying Pico SDK hardware libraries needed by our shared files
target_link_libraries(shared PUBLIC pico_stdlib hardware_spi hardware_gpio)