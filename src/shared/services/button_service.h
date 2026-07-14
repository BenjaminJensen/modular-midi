#pragma once

#include "shared/button.h"
#include "shared/hal/pin_concept.h"
#include "FreeRTOS.h"
#include "task.h"

template<GpioPin PinT>
class ButtonService {
public:
    ButtonService() {};

    void add_button(Button<PinT>* button) {
        if (m_button_count < MAX_BUTTONS) {
            m_buttons[m_button_count++] = button;
        }
    }

    void update(uint8_t delta_time) {
        for (uint8_t i = 0; i < m_button_count; i++) {
            m_buttons[i]->update(delta_time);
        }
    }

    void start(UBaseType_t priority) {
        xTaskCreateStatic(
            task_entry,          // Task entry function
            "ButtonService",     // Name of the task (for debugging)
            stack_size,         // Stack size in words
            this,               // Parameter to pass to the task (pointer to this instance)
            priority,           // Task priority
            xStack,            // Stack buffer (not needed for dynamic allocation)
            &xTaskBuffer             // Task control block (not needed for dynamic allocation)
        );
    }

private:
    static void task_entry(void* pvParameters) {
        auto* instance = static_cast<ButtonService*>(pvParameters);
        instance->run();
    }

    void run() {
        const uint8_t UPDATE_INTERVAL_MS = 10;
        for(;;) {
            update(UPDATE_INTERVAL_MS);
            vTaskDelay(pdMS_TO_TICKS(UPDATE_INTERVAL_MS));
        }
    }

    const static uint16_t stack_size = 512;
    StackType_t xStack[stack_size];
    StaticTask_t xTaskBuffer;

    static const uint8_t MAX_BUTTONS = 4;
    Button<PinT>* m_buttons[MAX_BUTTONS];
    uint8_t m_button_count = 0;
};
