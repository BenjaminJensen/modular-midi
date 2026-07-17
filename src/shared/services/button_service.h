#pragma once

#include <array>
#include <cstdint>
#include "shared/button.h"
#include "shared/hal/log_sink_concept.h"
#include "shared/hal/pin_concept.h"
#include "shared/hal/task_runner_concept.h"

template<GpioPin PinT, LogSink SinkT, TaskRunner RunnerT>
class ButtonService {
public:
    explicit ButtonService(RunnerT& runner) : m_runner(runner) {}

    void add_button(Button<PinT, SinkT>* button) {
        if (m_button_count < MAX_BUTTONS) {
            m_buttons[m_button_count++] = button;
        }
    }

    void update(uint8_t delta_time) {
        for (uint8_t i = 0; i < m_button_count; i++) {
            m_buttons[i]->update(delta_time);
        }
    }

    void start() {
        m_runner.start(&task_entry, this);
    }

private:
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
    std::array<Button<PinT, SinkT>*, MAX_BUTTONS> m_buttons{};
    uint8_t m_button_count = 0;
};
