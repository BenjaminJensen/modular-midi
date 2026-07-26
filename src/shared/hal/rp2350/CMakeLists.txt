# shared/hal/rp2350/CMakeLists.txt

set(RTT_PATH "${CMAKE_SOURCE_DIR}/src/libs/RTT/RTT")

target_sources(shared PRIVATE
    spi_dma_bus.cpp
    pio_spi_bus.cpp
    rtt_sink.cpp
    logger_instance.cpp
    ${RTT_PATH}/SEGGER_RTT.c        # Much cleaner
    ${RTT_PATH}/SEGGER_RTT_printf.c
)

pico_generate_pio_header(shared ${CMAKE_CURRENT_LIST_DIR}/st7789_spi.pio)

# This allows files in 'shared' or 'services' to #include "display.h", "pin.h", etc.
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
    hardware_pio
    pico_stdlib
)