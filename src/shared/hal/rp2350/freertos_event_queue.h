#pragma once

#include <array>
#include <cstdint>
#include "FreeRTOS.h"
#include "queue.h"
#include "shared/event/event_common.h"

// FreeRTOS-backed EventQueue: owns the static storage + queue control block
// for one queue of Events. Capacity is the number of Events it can hold, known
// at compile time - no dynamic allocation is used anywhere in this firmware.
// send() uses a zero timeout (never blocks the producer) - a full queue drops
// the newest event, which is the policy documented in architecture/EVENT_SYSTEM.md.
template<uint32_t Capacity>
class FreeRTOSEventQueue {
public:
    FreeRTOSEventQueue() {
        m_handle = xQueueCreateStatic(Capacity, sizeof(Event), m_storage.data(), &m_queue);
    }

    FreeRTOSEventQueue(const FreeRTOSEventQueue&) = delete;
    FreeRTOSEventQueue& operator=(const FreeRTOSEventQueue&) = delete;

    bool send(const Event& item) {
        return xQueueSend(m_handle, &item, 0) == pdTRUE;
    }

    bool receive(Event& out, uint32_t timeout_ms) {
        return xQueueReceive(m_handle, &out, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
    }

private:
    std::array<uint8_t, Capacity * sizeof(Event)> m_storage{};
    StaticQueue_t m_queue{};
    QueueHandle_t m_handle = nullptr;
};
