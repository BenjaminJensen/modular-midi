# CMake build instructions for the hub-master sub-project

# Define the executable and its source files
add_executable(hub_master 
    main.cpp
)

# Add include directories (where your headers live)
target_include_directories(hub_master PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}
)

# Disable default stdio drivers since we use our custom RTT logger
pico_enable_stdio_usb(hub_master 0)
pico_enable_stdio_uart(hub_master 0)

# Link the actual libraries, including the shared interface library
target_link_libraries(hub_master PRIVATE
    pico_stdlib 
    FreeRTOS-Kernel-Heap4
    shared
    pico_multicore
)

# Create ELF, UF2, etc.
pico_add_extra_outputs(hub_master)