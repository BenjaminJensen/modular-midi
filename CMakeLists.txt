cmake_minimum_required(VERSION 3.13)

# 1. Pull in the Pico SDK hardware abstraction
# This uses the environment variable set in your PowerShell script
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(modular_midi_system C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# 2. Initialize the SDK for the RP2350
pico_sdk_init()

# 3. Add your sub-projects
# This keeps the Hub and Satellite builds clean and separate
add_subdirectory(projects/hub-master)
# add_subdirectory(projects/satellite-node) # Enable when ready