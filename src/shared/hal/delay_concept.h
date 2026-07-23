#pragma once

#include <concepts>
#include <cstdint>

// A blocking delay source: anything exposing these two operations satisfies
// this, no base class required. delay_ms() blocks for at least the given
// duration - used for datasheet-mandated settle times (reset pulses, sleep-out).
// spin_hint() is a separate, much cheaper operation called once per iteration
// of a busy-poll loop (e.g. waiting on a DMA-completion flag) - it must not
// itself sleep, or it would slow down detection of the very thing being polled.
template<typename T>
concept Delay = requires(T& d, uint32_t ms) {
    { d.delay_ms(ms) } -> std::same_as<void>;
    { d.spin_hint() } -> std::same_as<void>;
};
