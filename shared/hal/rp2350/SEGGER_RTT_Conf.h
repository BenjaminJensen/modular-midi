#pragma once

#include "hardware/sync.h"

// -----------------------------------------------------------------------------
// RTT Buffer Configuration
// -----------------------------------------------------------------------------
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS       (3)  // 0=printf, 1=Core0, 2=Core1
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS     (1)

#define BUFFER_SIZE_UP                      (1024)
#define BUFFER_SIZE_DOWN                    (16)
#define SEGGER_RTT_PRINTF_BUFFER_SIZE       (256)

// -----------------------------------------------------------------------------
// RTT Hardware Lock Configuration (Multi-core & IRQ safe)
// -----------------------------------------------------------------------------
// To allow logging from both cores and their ISRs safely, we wrap the internal 
// RTT operations in a Pico SDK hardware spinlock.

#ifdef __cplusplus
extern "C" {
#endif
    extern spin_lock_t* rtt_spin_lock;
#ifdef __cplusplus
}
#endif

#define SEGGER_RTT_LOCK()   uint32_t _rtt_saved_irq = rtt_spin_lock ? spin_lock_blocking(rtt_spin_lock) : 0
#define SEGGER_RTT_UNLOCK() do { if (rtt_spin_lock) spin_unlock(rtt_spin_lock, _rtt_saved_irq); } while(0)