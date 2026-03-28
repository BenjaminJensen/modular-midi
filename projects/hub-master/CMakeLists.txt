add_executable(hub_master
    main.cpp
)

# Pull in the standard Pico SDK library
target_link_libraries(hub_master pico_stdlib)

# IMPORTANT: Enable USB output, Disable hardware UART output
pico_enable_stdio_usb(hub_master 1)
pico_enable_stdio_uart(hub_master 0)

# We ONLY want the .elf file for the J-Link. 
# This skips the checksum/UF2 stage that requires picotool.
# pico_add_extra_outputs(hub_master) <- COMMENT THIS OUT

# This generates the .uf2 file for drag-and-drop flashing
#pico_add_extra_outputs(hub_master)