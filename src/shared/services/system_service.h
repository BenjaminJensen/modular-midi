#pragma once

#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>
#include "shared/event/event_common.h"
#include "shared/event/event_queue_concept.h"
#include "shared/hal/log_sink_concept.h"
#include "shared/hal/task_runner_concept.h"
#include "shared/logger.h"
#include "shared/render/color_palette.h"
#include "shared/render/font_size.h"
#include "shared/render/label.h"
#include "shared/render/mailbox.h"
#include "shared/services/button_payload.h"

// SystemService owns overall system state, derived from events published by other
// Services. For now it only consumes Button events, logs them, and mirrors the
// latest one onto display 0 via the Mailbox (see architecture/EVENT_SYSTEM.md,
// docs/adr/0001) - future state (active program, etc.) is intended to grow here
// without needing to restructure this consumer-side wiring.
template<LogSink SinkT, TaskRunner RunnerT, EventQueue<Event> QueueT, uint8_t DisplayCount>
class SystemService {
public:
    SystemService(Logger<SinkT>& logger, RunnerT& runner, QueueT& queue, Mailbox<DisplayCount>& mailbox)
        : m_logger(&logger), m_runner(runner), m_queue(queue), m_mailbox(mailbox) {}

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
                m_mailbox.write(BUTTON_DISPLAY_ID, make_button_label(payload));
                break;
            }
            default:
                break;
        }
    }

    // Builds "B<id>: <State>" (e.g. "B0: Pressed") without heap allocation, per
    // Label's own fixed-buffer, no-dynamic-allocation contract.
    static Label make_button_label(ButtonPayload payload) {
        std::array<char, Label::MAX_TEXT_LENGTH + 1> buffer{};
        size_t pos = 0;
        buffer[pos++] = 'B';

        auto id_result = std::to_chars(buffer.data() + pos, buffer.data() + buffer.size(), payload.id);
        pos = static_cast<size_t>(id_result.ptr - buffer.data());

        pos = append(buffer, pos, ": ");
        pos = append(buffer, pos, to_label_string(payload.state));

        return Label::make(std::string_view(buffer.data(), pos), ColorPalette::Default, FontSize::Small);
    }

    static size_t append(std::array<char, Label::MAX_TEXT_LENGTH + 1>& buffer, size_t pos, std::string_view text) {
        size_t available = buffer.size() - pos;
        size_t to_copy = text.size() < available ? text.size() : available;
        for (size_t i = 0; i < to_copy; ++i) {
            buffer[pos + i] = text[i];
        }
        return pos + to_copy;
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
    static constexpr uint8_t BUTTON_DISPLAY_ID = 0;

    Logger<SinkT>* m_logger;
    RunnerT& m_runner;
    QueueT& m_queue;
    Mailbox<DisplayCount>& m_mailbox;
};
