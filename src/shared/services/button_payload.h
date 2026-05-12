#pragma once
#include <stdint.h>
#include "shared/event/event_common.h"

struct ButtonPayload {
    uint8_t id;
    uint8_t state;
    uint8_t reserved;

    // Use a constructor for internal logic
    constexpr ButtonPayload(uint8_t i, uint8_t s, uint8_t r = 0) 
        : id(i), state(s), reserved(r) {}

    static constexpr ButtonPayload unpack(uint32_t payload) {
        return ButtonPayload(
            static_cast<uint8_t>(payload >> 16),
            static_cast<uint8_t>(payload >> 8),
            static_cast<uint8_t>(payload & 0xFF)
        );
    }
    
    // A static helper to turn this into a generic Event
    static constexpr Event pack(uint8_t id, uint8_t state) {
        uint32_t data = (id << 16) | (state << 8);
        return { (static_cast<uint32_t>(EventType::Button) << 24) | data };
    }
};