#pragma once
#include <stdint.h>
#include "shared/event/event_common.h"

enum class ButtonEventState : uint8_t { Pressed, Released, LongPressed, DoubleTapped };

// Matches the wording Button itself already logs (e.g. "Button 3 pressed"), so a
// button's own debug lines and any consumer's event-driven log lines read consistently.
constexpr const char* to_string(ButtonEventState state) {
    switch (state) {
        case ButtonEventState::Pressed:      return "pressed";
        case ButtonEventState::Released:     return "released";
        case ButtonEventState::LongPressed:  return "long press";
        case ButtonEventState::DoubleTapped: return "double tap";
    }
    return "unknown";
}

// Enum-member-name wording (as opposed to to_string()'s log-line wording above),
// for consumers that want the state rendered verbatim, e.g. a display label.
constexpr const char* to_label_string(ButtonEventState state) {
    switch (state) {
        case ButtonEventState::Pressed:      return "Pressed";
        case ButtonEventState::Released:     return "Released";
        case ButtonEventState::LongPressed:  return "LongPressed";
        case ButtonEventState::DoubleTapped: return "DoubleTapped";
    }
    return "Unknown";
}

struct ButtonPayload {
    uint8_t id;
    ButtonEventState state;
    uint8_t reserved;

    // Use a constructor for internal logic
    constexpr ButtonPayload(uint8_t i, ButtonEventState s, uint8_t r = 0)
        : id(i), state(s), reserved(r) {}

    static constexpr ButtonPayload unpack(uint32_t payload) {
        return {
            static_cast<uint8_t>(payload >> 16),
            static_cast<ButtonEventState>((payload >> 8) & 0xFF),
            static_cast<uint8_t>(payload & 0xFF)
        };
    }

    // A static helper to turn this into a generic Event
    static constexpr Event pack(uint8_t id, ButtonEventState state) {
        uint32_t data = (static_cast<uint32_t>(id) << 16) | (static_cast<uint32_t>(state) << 8);
        return { (static_cast<uint32_t>(EventType::Button) << 24) | data };
    }
};
