#pragma once

#include <concepts>
#include <string_view>

// A log output destination: anything exposing write(std::string_view) satisfies
// this, no base class required. write() receives one complete, already-formatted
// log line (no trailing newline) and is expected to emit it in a single call, so
// that the sink can make that call atomic with respect to other writers.
template<typename T>
concept LogSink = requires(T& sink, std::string_view text) {
    { sink.write(text) } -> std::same_as<void>;
};
