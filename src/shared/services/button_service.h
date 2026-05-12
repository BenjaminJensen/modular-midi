#pragma once

#include "shared/button.h"
#include "FreeRTOS.h"
#include "task.h"


class ButtonService {
public:
    ButtonService() {};

    void add_button(Button* button);
    void update(uint8_t delta_time);
    void start(UBaseType_t priority);

private:

   static void task_entry(void*pvParameters);
    void run();
    const static uint16_t stack_size = 512;
    StackType_t xStack[stack_size];
    StaticTask_t xTaskBuffer;

    static const uint8_t MAX_BUTTONS = 4;
    Button* m_buttons[MAX_BUTTONS];
    uint8_t m_button_count = 0;
};