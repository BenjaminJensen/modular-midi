# shared/hal/drivers/CMakeLists.txt

# Drivers here (e.g. ST7789) are written entirely against hal/ concepts
# (SpiBus, GpioOutputPin, Delay, ...) with no dependency, direct or
# transitive, on any specific platform's SDK - unlike shared/hal/rp2350/,
# which holds the concrete implementations of those concepts. They're
# header-only templates, so there is no .cpp to add as a source here.

# This allows files in 'shared' to #include "st7789.h" without needing
# the full relative path.
target_include_directories(shared PUBLIC ${CMAKE_CURRENT_LIST_DIR})
