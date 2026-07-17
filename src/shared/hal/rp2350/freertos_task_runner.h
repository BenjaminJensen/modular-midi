#pragma once

#include <array>
#include <cstdint>
#include "FreeRTOS.h"
#include "task.h"

// FreeRTOS-backed TaskRunner: owns the static stack + TCB for one task.
// StackWords is the stack depth in words (FreeRTOS's native unit for
// xTaskCreateStatic's uStackDepth), so it must be known at compile time -
// no dynamic allocation is used anywhere in this firmware.
template<uint32_t StackWords>
class FreeRTOSTaskRunner {
public:
    FreeRTOSTaskRunner(const char* name, UBaseType_t priority)
        : m_name(name), m_priority(priority) {}

    FreeRTOSTaskRunner(const FreeRTOSTaskRunner&) = delete;
    FreeRTOSTaskRunner& operator=(const FreeRTOSTaskRunner&) = delete;

    void start(void (*entry)(void*), void* context) {
        xTaskCreateStatic(entry, m_name, StackWords, context, m_priority, m_stack.data(), &m_tcb);
    }

    void delay_ms(uint32_t ms) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

private:
    const char* m_name;
    UBaseType_t m_priority;
    std::array<StackType_t, StackWords> m_stack;
    StaticTask_t m_tcb;
};
