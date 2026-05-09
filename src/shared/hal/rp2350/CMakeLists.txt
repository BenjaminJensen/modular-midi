# shared/hal/rp2350/CMakeLists.txt

set(RTT_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../libs/RTT/RTT")

target_sources(shared PRIVATE
    st7789.cpp
    async_logger.cpp
    display.cpp
    ${RTT_PATH}/SEGGER_RTT.c        # Much cleaner
    ${RTT_PATH}/SEGGER_RTT_printf.c
)

# This allows files in 'shared' or 'services' to #include "st7789.h" 
# without needing the full relative path.
target_include_directories(shared PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR}
    ${RTT_PATH}
)


target_link_libraries(shared PUBLIC 
    pico_stdlib 
    hardware_spi 
    hardware_gpio 
    hardware_dma 
    pico_stdlib 
)