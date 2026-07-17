
#pragma once

#include <cstdint>
#include "hal/log_sink_concept.h"
#include "hal/pin_concept.h"
#include "logger.h"

// What happened during a single update() call - a bitmask, since a double tap
// resolves on the same tick as the release edge that completes it (both bits
// set in one return value).
enum class ButtonTransition : uint8_t {
    None         = 0,
    Pressed      = 1 << 0,
    Released     = 1 << 1,
    LongPressed  = 1 << 2,
    DoubleTapped = 1 << 3,
};

constexpr ButtonTransition operator|(ButtonTransition a, ButtonTransition b) {
    return static_cast<ButtonTransition>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
constexpr ButtonTransition operator&(ButtonTransition a, ButtonTransition b) {
    return static_cast<ButtonTransition>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
constexpr ButtonTransition& operator|=(ButtonTransition& a, ButtonTransition b) {
    a = a | b;
    return a;
}
[[nodiscard]] constexpr bool has(ButtonTransition transitions, ButtonTransition flag) {
    return (transitions & flag) != ButtonTransition::None;
}

template<GpioPin PinT, LogSink SinkT>
class Button {
public:
    Button(uint8_t id, PinT* pin, Logger<SinkT>& logger, uint16_t long_press_ms = 1000)
        : m_id(id), m_pin(pin), m_logger(&logger), m_long_press_ms(long_press_ms) {}

    /*
        The update function takes a deltatiome in milliseconds since the last call.
        It should be called in a regular loop (e.g., every 10ms) to process the button state and generate events based on the pin readings.
        The function handles debouncing, long press detection, and double tap detection, returning
        whichever of those happened during this call (if any).
    */
    [[nodiscard]] ButtonTransition update(uint8_t delta_time) {
        ButtonTransition transitions = ButtonTransition::None;
        bool current_reading = m_pin->read();

        // Debouncing via bit-shifting
        if(false) {
            // Active HIGH: pressed = 1, released = 0
            m_state = (m_state << 1) | (current_reading ? 1 : 0);
        }
        else {
            // Active LOW: pressed = 0, released = 1
            m_state = (m_state << 1) | (current_reading ? 0 : 1);
        }

        // Evaluate Debounced State
        // 0x0F means 4 consecutive "pressed" readings
        if ((m_state & 0x0F) == 0x0F && !m_is_pressed) {
            m_is_pressed = true;
            m_hold_timer = 0;
            m_long_press_triggered = false;
            m_logger->debug() << "Button " << m_id << " pressed";
            transitions |= ButtonTransition::Pressed;
        }
        else if ((m_state & 0x0F) == 0x00 && m_is_pressed) {
            // 0xF0 means 4 consecutive "released" readings
            m_is_pressed = false;
            m_logger->debug() << "Button " << m_id << " released";
            transitions |= ButtonTransition::Released;

            if (!m_long_press_triggered) {
                m_tap_count++;
                m_gap_timer = 0; // Start window for double tap
            }
        }

        // Logic for Long Press (while held)
        if (m_is_pressed && !m_long_press_triggered) {
            m_hold_timer += delta_time;
            if (m_hold_timer >= m_long_press_ms) {
                m_long_press_triggered = true;
                m_tap_count = 0; // Cancel double tap if it was a long press
                m_logger->debug() << "Button " << m_id << " long press";
                transitions |= ButtonTransition::LongPressed;
            }
        }

        // Logic for Double Tap (after release)
        if (!m_is_pressed && m_tap_count > 0) {
           m_gap_timer += delta_time;

            if (m_tap_count >= 2) {
                m_tap_count = 0;
                m_logger->debug() << "Button " << m_id << " double tap";
                transitions |= ButtonTransition::DoubleTapped;
            }
            else if (m_gap_timer >= m_long_press_ms) {
                // If the gap expires and we only have 1 tap, it was just a single tap
                // (Optional: trigger a SINGLE_TAP event here if needed)
                m_tap_count = 0;
            }
        }

        return transitions;
    }

    [[nodiscard]] bool is_pressed() const { return m_is_pressed; }
    [[nodiscard]] uint8_t id() const { return m_id; }

private:
    uint8_t m_id;
    PinT* m_pin;
    Logger<SinkT>* m_logger;
    uint8_t m_state = 0;
    bool m_is_pressed = false;
    uint16_t m_hold_timer = 0;
    bool m_long_press_triggered = false;
    uint8_t m_tap_count = 0;
    uint16_t m_gap_timer = 0;
    uint16_t m_long_press_ms;
};


/*
-ButtonService
-- Button
--- Pin


*/
