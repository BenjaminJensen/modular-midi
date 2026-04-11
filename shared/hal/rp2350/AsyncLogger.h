#pragma once

#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "SEGGER_RTT.h"
#include <cstdarg>

extern "C" {
    extern spin_lock_t* rtt_spin_lock;
}

class Logger {
private:
    static bool initialized;

public:
    static void init() {
        if (initialized) return;

        // Claim an unused hardware spinlock for the internal SEGGER RTT locks
        int lock_num = spin_lock_claim_unused(true);
        rtt_spin_lock = spin_lock_instance(lock_num);

        // Configure RTT channels
        SEGGER_RTT_ConfigUpBuffer(1, "Core0", NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
        SEGGER_RTT_ConfigUpBuffer(2, "Core1", NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);

        initialized = true;
    }

    // Direct formatted logging, fully safe for pointers and dynamic strings
    static void log(const char* format, ...) {
        if (!initialized) return;

        va_list args;
        va_start(args, format);

        uint core_num = get_core_num();
        
        if (core_num == 0) {
            SEGGER_RTT_vprintf(1, format, &args);
        } else {
            SEGGER_RTT_vprintf(2, format, &args);
        }

        
        va_end(args);
    }
};
