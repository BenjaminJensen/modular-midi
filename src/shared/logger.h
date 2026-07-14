#pragma once

#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "hal/log_sink_concept.h"

enum class LogLevel : uint8_t { Debug, Info, Warn, Error };

// One log line, built up via operator<< and flushed to the sink in a single
// write() call when it goes out of scope. Formats into a fixed-size stack
// buffer -- no heap allocation. Non-copyable; move-only so a moved-from
// instance doesn't also flush.
template<LogSink SinkT>
class LogStream {
public:
    LogStream(SinkT& sink, LogLevel level, LogLevel min_level)
        : m_sink(&sink), m_active(level >= min_level) {}

    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;
    LogStream& operator=(LogStream&&) = delete;

    LogStream(LogStream&& other) noexcept
        : m_sink(other.m_sink), m_active(other.m_active), m_buffer(other.m_buffer), m_length(other.m_length) {
        other.m_active = false;
    }

    ~LogStream() {
        if (m_active && m_length > 0) {
            m_sink->write(std::string_view(m_buffer.data(), m_length));
        }
    }

    LogStream& operator<<(std::string_view text) {
        append(text);
        return *this;
    }

    LogStream& operator<<(const char* text) {
        return *this << std::string_view(text);
    }

    LogStream& operator<<(char c) {
        append(std::string_view(&c, 1));
        return *this;
    }

    LogStream& operator<<(bool value) {
        return *this << std::string_view(value ? "true" : "false");
    }

    template<std::integral T>
    LogStream& operator<<(T value) {
        std::array<char, 24> digits{};
        auto result = std::to_chars(digits.data(), digits.data() + digits.size(), value);
        append(std::string_view(digits.data(), static_cast<size_t>(result.ptr - digits.data())));
        return *this;
    }

    template<std::floating_point T>
    LogStream& operator<<(T value) {
        std::array<char, 32> digits{};
        auto result = std::to_chars(digits.data(), digits.data() + digits.size(), value);
        append(std::string_view(digits.data(), static_cast<size_t>(result.ptr - digits.data())));
        return *this;
    }

private:
    void append(std::string_view text) {
        if (!m_active) return;
        size_t available = m_buffer.size() - m_length;
        size_t to_copy = text.size() < available ? text.size() : available;
        for (size_t i = 0; i < to_copy; ++i) {
            m_buffer[m_length + i] = text[i];
        }
        m_length += to_copy;
    }

    static constexpr size_t BUFFER_SIZE = 160;

    SinkT* m_sink;
    bool m_active;
    std::array<char, BUFFER_SIZE> m_buffer{};
    size_t m_length = 0;
};

// Hardware-independent facade: constructed around anything satisfying LogSink.
template<LogSink SinkT>
class Logger {
public:
    explicit Logger(SinkT& sink) : m_sink(sink) {}

    LogStream<SinkT> debug() { return LogStream<SinkT>(m_sink, LogLevel::Debug, m_min_level); }
    LogStream<SinkT> info() { return LogStream<SinkT>(m_sink, LogLevel::Info, m_min_level); }
    LogStream<SinkT> warn() { return LogStream<SinkT>(m_sink, LogLevel::Warn, m_min_level); }
    LogStream<SinkT> error() { return LogStream<SinkT>(m_sink, LogLevel::Error, m_min_level); }

    void set_min_level(LogLevel level) { m_min_level = level; }

private:
    SinkT& m_sink;
    LogLevel m_min_level = LogLevel::Debug;
};
