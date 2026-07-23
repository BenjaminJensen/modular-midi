#pragma once
#include <cstdint>

// LVGL needs specific compiled-in font objects, not arbitrary point sizes - see
// docs/adr/0001. Large is first so a zero-initialized FontSize matches lv_conf.h's
// current LV_FONT_DEFAULT (Montserrat 42).
enum class FontSize : uint8_t { Large, Small };
