#pragma once
#include <cstdint>

// Named palette rather than raw RGB - see docs/adr/0001. Meaning (what "Active"
// actually looks like) is centralized in the Render Engine, not chosen per-call.
enum class ColorPalette : uint8_t { Default, Active };
