#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>

// What an LVGL glue layer (see rp2350/display.h) needs from a physical
// display driver: start an async pixel-burst write for a rectangle, and
// report whether that burst is still in flight. Satisfied by ST7789 today;
// deliberately has no notion of LVGL types, so any future driver just needs
// these two operations.
template<typename T>
concept DisplayDriver = requires(T& drv, const uint16_t* data, size_t len,
                                  int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    { drv.update(data, len, x1, y1, x2, y2) } -> std::same_as<void>;
    { drv.is_busy() } -> std::convertible_to<bool>;
};
