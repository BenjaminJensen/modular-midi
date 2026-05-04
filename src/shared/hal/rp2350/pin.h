#pragma once

#include <cstdint>
#include "i_pin.h"
#include "pico/stdlib.h"

class Pin : public IPin {
public:
    Pin(uint8_t pin_number) : m_pin_number(pin_number) {
        // Initialize the GPIO pin for input
        gpio_init(m_pin_number);
        gpio_set_dir(m_pin_number, GPIO_IN);
        // Enable pull-up resistor
        gpio_pull_up(m_pin_number); 
    }

    bool read() override {
        return gpio_get(m_pin_number);
    }

private:
    uint8_t m_pin_number;
};