#include "button_service.h"
#include "AsyncLogger.h"

void ButtonService::start(UBaseType_t priority) {
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

void ButtonService::task_entry(void*pvParameters) {
// Cast the parameter back to the class instance and run the loop
    auto* instance = static_cast<ButtonService*>(pvParameters);
    instance->run();
}
void ButtonService::run() {
    const uint8_t UPDATE_INTERVAL_MS = 10;
    for(;;) {
        //LOG_DEBUG("ButtonService update\n");
        update(UPDATE_INTERVAL_MS);
        vTaskDelay(pdMS_TO_TICKS(UPDATE_INTERVAL_MS));
    }
}

void ButtonService::add_button(Button* button) {
    if (m_button_count < MAX_BUTTONS) {
        m_buttons[m_button_count++] = button;
    }
}

void ButtonService::update(uint8_t delta_time) {
    for (uint8_t i = 0; i < m_button_count; i++) {
        m_buttons[i]->update(delta_time);
    }
}