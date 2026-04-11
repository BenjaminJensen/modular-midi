# Create a STATIC library to compile our shared source files
add_library(shared STATIC
    hal/rp2350/st7789.cpp
    hal/rp2350/AsyncLogger.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../libs/RTT/RTT/SEGGER_RTT.c
    ${CMAKE_CURRENT_LIST_DIR}/../libs/RTT/RTT/SEGGER_RTT_printf.c
)

target_include_directories(shared PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/hal/rp2350
    ${CMAKE_CURRENT_LIST_DIR}/../libs/RTT/RTT
)

# Link the underlying Pico SDK hardware libraries needed by our shared files
target_link_libraries(shared PUBLIC pico_stdlib hardware_spi hardware_gpio)