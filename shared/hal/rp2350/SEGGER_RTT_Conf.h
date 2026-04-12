#pragma once

#include "hardware/sync.h"

// -----------------------------------------------------------------------------
// RTT Buffer Configuration
// -----------------------------------------------------------------------------
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS       (1)  // BlackMagic Probe unly supports channel 0 for up buffers, so we only configure one up buffer
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS     (1)

#define BUFFER_SIZE_UP                      (1024)
#define BUFFER_SIZE_DOWN                    (16)
#define SEGGER_RTT_PRINTF_BUFFER_SIZE       (256)

// -----------------------------------------------------------------------------
// RTT Hardware Lock Configuration (Multi-core & IRQ safe)
// -----------------------------------------------------------------------------
// To allow logging from both cores and their ISRs safely, we wrap the internal 
// RTT operations in a Pico SDK hardware spinlock.

#define SEGGER_RTT_LOCK()   
#define SEGGER_RTT_UNLOCK()