#pragma once

#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "SEGGER_RTT.h"
#include <cstdarg>
#define ENABLE_LOGGING
#ifdef ENABLE_LOGGING
    #define LOG_DEBUG(fmt, ...) Logger::log(fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...) ((void)0)
#endif

extern "C" {
    extern spin_lock_t* rtt_spin_lock;
}

class Logger {
private:
    static bool initialized;

public:
    static void init();

    // Direct formatted logging, fully safe for pointers and dynamic strings
    static void log(const char* format, ...) {
        if (!initialized || !rtt_spin_lock) return;

        // Gem interrupts og tag locken for at sikre, at den anden kerne venter
        uint32_t save = spin_lock_blocking(rtt_spin_lock);

        va_list args;
        va_start(args, format);

        uint core_num = get_core_num();

        // Nu ejer denne kerne RTT bufferen fuldstændigt
        if (core_num == 0) {
            SEGGER_RTT_WriteString(0, "0: ");
        } else {
            SEGGER_RTT_WriteString(0, "1: ");
        }

        SEGGER_RTT_vprintf(0, format, &args);

        va_end(args);

        // Giv locken fri og genaktiver interrupts
        spin_unlock(rtt_spin_lock, save);
    }
};
