#pragma once

#include <cstdint>
#include "pico/stdlib.h"

class OutputPin {
public:
    explicit OutputPin(uint8_t pin_number, bool initial_level = true) : m_pin_number(pin_number) {
        gpio_init(m_pin_number);
        gpio_set_dir(m_pin_number, GPIO_OUT);
        gpio_put(m_pin_number, initial_level);
    }

    void write(bool level) {
        gpio_put(m_pin_number, level);
    }

private:
    uint8_t m_pin_number;
};
