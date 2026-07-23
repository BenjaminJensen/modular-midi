#pragma once
#include <stdint.h>

// Common_Event.hpp (The only thing everyone includes)
enum class EventType : uint8_t {
    Button = 0,
    Midi = 1,
    UI = 2, // Reserved, deliberately unassigned - see docs/adr/0002
    State = 3, // Application/system-level state transitions (program loaded, mode changed, ...)
    CustomStart = 100 // Allows new modules to claim IDs
};

struct Event {
    uint32_t raw;

    [[nodiscard]] constexpr EventType type() const { 
        return static_cast<EventType>(raw >> 24); 
    }
    [[nodiscard]] constexpr uint32_t payload() const { 
        return raw & 0x00FFFFFF; 
    }
};