#pragma once

#include <concepts>
#include <cstdint>

// A fixed-capacity queue of Events: anything exposing send(item)/receive(out, timeout_ms)
// with these signatures satisfies this, no base class required - so it can be backed by
// something other than FreeRTOS on the host (see tests/mocks/fake_event_queue.h).
// send() returning false means the queue was full and the item was dropped; receive()
// returning false means it timed out with nothing available.
template<typename Q, typename T>
concept EventQueue = requires(Q& q, const T& item, T& out, uint32_t timeout_ms) {
    { q.send(item) } -> std::same_as<bool>;
    { q.receive(out, timeout_ms) } -> std::same_as<bool>;
};
