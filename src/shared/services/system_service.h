#pragma once

#include <cstdint>
#include "shared/event/event_common.h"
#include "shared/event/event_queue_concept.h"
#include "shared/hal/log_sink_concept.h"
#include "shared/hal/task_runner_concept.h"
#include "shared/logger.h"
#include "shared/services/button_payload.h"

// SystemService owns overall system state, derived from events published by other
// Services. For now it only consumes Button events and logs them - future state
// (active program, etc.) is intended to grow here without needing to restructure
// this consumer-side wiring.
template<LogSink SinkT, TaskRunner RunnerT, EventQueue<Event> QueueT>
class SystemService {
public:
    SystemService(Logger<SinkT>& logger, RunnerT& runner, QueueT& queue)
        : m_logger(&logger), m_runner(runner), m_queue(queue) {}

    // Drains and handles every event currently queued. Directly callable from tests
    // (see run()'s blocking-receive loop for the real runtime driver).
    void update() {
        Event event;
        while (m_queue.receive(event, 0)) {
            handle(event);
        }
    }

    void start() {
        m_runner.start(&task_entry, this);
    }

private:
    void handle(Event event) {
        switch (event.type()) {
            case EventType::Button: {
                ButtonPayload payload = ButtonPayload::unpack(event.payload());
                m_logger->debug() << "event: button " << payload.id << " " << to_string(payload.state);
                break;
            }
            default:
                break;
        }
    }

    static void task_entry(void* pvParameters) {
        auto* instance = static_cast<SystemService*>(pvParameters);
        instance->run();
    }

    void run() {
        for (;;) {
            Event event;
            if (m_queue.receive(event, POLL_TIMEOUT_MS)) {
                handle(event);
            }
        }
    }

    static constexpr uint32_t POLL_TIMEOUT_MS = 100;

    Logger<SinkT>* m_logger;
    RunnerT& m_runner;
    QueueT& m_queue;
};
