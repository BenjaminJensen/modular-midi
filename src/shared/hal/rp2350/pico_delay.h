#pragma once

#include <cstdint>
#include "pico/stdlib.h"

// Delay implementation for contexts with no FreeRTOS scheduler to yield to
// (e.g. display_task's bare loop on core 1) - wraps pico-sdk's blocking
// sleep_ms()/tight_loop_contents() directly, unlike FreeRTOSTaskRunner's
// delay_ms() which calls vTaskDelay() and requires the scheduler to be running
// on that core.
class PicoDelay {
public:
    void delay_ms(uint32_t ms) {
        sleep_ms(ms);
    }

    void spin_hint() {
        tight_loop_contents();
    }
};
