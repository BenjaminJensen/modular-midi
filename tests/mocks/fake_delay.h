#pragma once

#include <cstdint>

class FakeDelay {
public:
    uint32_t total_delay_ms = 0;
    int spin_hint_count = 0;

    void delay_ms(uint32_t ms) {
        total_delay_ms += ms;
    }

    void spin_hint() {
        ++spin_hint_count;
    }
};
