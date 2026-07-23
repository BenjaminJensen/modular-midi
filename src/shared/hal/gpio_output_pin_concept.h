#pragma once

#include <concepts>

// A GPIO-style output pin: anything exposing void write(bool) satisfies this,
// no base class required. write(true) drives the pin HIGH, write(false) drives it LOW.
template<typename T>
concept GpioOutputPin = requires(T& pin, bool level) {
    { pin.write(level) } -> std::same_as<void>;
};
