#pragma once

#include <cstdint>

// Host-side TaskRunner double: records what start() was called with instead of
// spawning anything. Must never invoke started_entry itself - it's the real
// Service's for(;;) loop and would hang a test.
class FakeTaskRunner {
public:
    void (*started_entry)(void*) = nullptr;
    void* started_context = nullptr;
    uint32_t total_delay_ms = 0;

    void start(void (*entry)(void*), void* context) {
        started_entry = entry;
        started_context = context;
    }

    void delay_ms(uint32_t ms) {
        total_delay_ms += ms;
    }
};
