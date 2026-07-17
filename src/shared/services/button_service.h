#pragma once

#include <array>
#include <cstdint>
#include "shared/button.h"
#include "shared/event/event_common.h"
#include "shared/event/event_queue_concept.h"
#include "shared/hal/log_sink_concept.h"
#include "shared/hal/pin_concept.h"
#include "shared/hal/task_runner_concept.h"
#include "shared/services/button_payload.h"

template<GpioPin PinT, LogSink SinkT, TaskRunner RunnerT, EventQueue<Event> QueueT>
class ButtonService {
public:
    ButtonService(RunnerT& runner, QueueT& queue) : m_runner(runner), m_queue(queue) {}

    void add_button(Button<PinT, SinkT>* button) {
        if (m_button_count < MAX_BUTTONS) {
            m_buttons[m_button_count++] = button;
        }
    }

    void update(uint8_t delta_time) {
        for (uint8_t i = 0; i < m_button_count; i++) {
            ButtonTransition transitions = m_buttons[i]->update(delta_time);
            publish(m_buttons[i]->id(), transitions);
        }
    }

    void start() {
        m_runner.start(&task_entry, this);
    }

private:
    // send()'s bool return (false = queue full) isn't handled here yet - the
    // drop-and-log overflow policy is EventRouter's job (see architecture/EVENT_SYSTEM.md,
    // "Overflow Policy"), and EventRouter doesn't exist yet since there's no second
    // consumer to route to.
    void publish(uint8_t id, ButtonTransition transitions) {
        if (has(transitions, ButtonTransition::Pressed)) {
            m_queue.send(ButtonPayload::pack(id, ButtonEventState::Pressed));
        }
        if (has(transitions, ButtonTransition::Released)) {
            m_queue.send(ButtonPayload::pack(id, ButtonEventState::Released));
        }
        if (has(transitions, ButtonTransition::LongPressed)) {
            m_queue.send(ButtonPayload::pack(id, ButtonEventState::LongPressed));
        }
        if (has(transitions, ButtonTransition::DoubleTapped)) {
            m_queue.send(ButtonPayload::pack(id, ButtonEventState::DoubleTapped));
        }
    }

    static void task_entry(void* pvParameters) {
        auto* instance = static_cast<ButtonService*>(pvParameters);
        instance->run();
    }

    void run() {
        for (;;) {
            update(UPDATE_INTERVAL_MS);
            m_runner.delay_ms(UPDATE_INTERVAL_MS);
        }
    }

    static constexpr uint8_t UPDATE_INTERVAL_MS = 10;
    static constexpr uint8_t MAX_BUTTONS = 4;

    RunnerT& m_runner;
    QueueT& m_queue;
    std::array<Button<PinT, SinkT>*, MAX_BUTTONS> m_buttons{};
    uint8_t m_button_count = 0;
};
