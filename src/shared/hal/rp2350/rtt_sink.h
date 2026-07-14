#pragma once

#include <string_view>

#include "hardware/sync.h"

// LogSink implementation writing to SEGGER RTT channel 0. Every write() call
// is prefixed with the originating core number and is the sole code path
// allowed to call into SEGGER_RTT_Write* -- SEGGER_RTT_Conf.h's own
// SEGGER_RTT_LOCK()/UNLOCK() are deliberately no-ops, so mutual exclusion
// across cores/ISRs for RTT access depends entirely on the spinlock taken
// here.
class RttSink {
public:
    // Claims a hardware spinlock. Must be called once from main() after
    // basic hardware bring-up, not from a global constructor (the spinlock
    // hardware isn't guaranteed ready at static-init time).
    void init();

    void write(std::string_view text);

private:
    spin_lock_t* m_lock = nullptr;
};
