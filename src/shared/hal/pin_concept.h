#pragma once

#include <concepts>

// A GPIO-style input pin: anything exposing bool read() satisfies this,
// no base class required. Must return true if the pin is HIGH, false if LOW.
template<typename T>
concept GpioPin = requires(T& pin) {
    { pin.read() } -> std::convertible_to<bool>;
};
