#pragma once

#include <concepts>
#include <cstdint>

// An execution context a Service can run in: anything exposing start(entry, context)
// and delay_ms(ms) satisfies this, no base class required. start() creates and begins
// running a task that calls entry(context); delay_ms() is the cooperative sleep
// primitive a running task uses inside its own loop.
template<typename T>
concept TaskRunner = requires(T& runner, void (*entry)(void*), void* context, uint32_t ms) {
    { runner.start(entry, context) } -> std::same_as<void>;
    { runner.delay_ms(ms) } -> std::same_as<void>;
};
