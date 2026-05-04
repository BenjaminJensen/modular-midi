
#pragma once

#include <cstdint>
#include "i_pin.h"
#include "AsyncLogger.h"

class Button {
public:
    Button(IPin* pin, uint16_t long_press_ms = 1000) : m_pin(pin), m_long_press_ms(long_press_ms) {}
    
    /*
        The update function takes a deltatiome in milliseconds since the last call. 
        It should be called in a regular loop (e.g., every 10ms) to process the button state and generate events based on the pin readings. 
        The function handles debouncing, long press detection, and double tap detection.
    */
    void update(uint8_t delta_time) {
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
             // event = ButtonEvent::PRESS;
             LOG_DEBUG("Button Pressed\n");
        }
        else if ((m_state & 0x0F) == 0x00 && m_is_pressed) {
            // 0xF0 means 4 consecutive "released" readings
            m_is_pressed = false;

            if (!m_long_press_triggered) {
                m_tap_count++;
                m_gap_timer = 0; // Start window for double tap
            }
            //event = ButtonEvent::RELEASE;
            LOG_DEBUG("Button Released\n");
        }

        // Logic for Long Press (while held)
        if (m_is_pressed && !m_long_press_triggered) {
            m_hold_timer += delta_time;
            if (m_hold_timer >= m_long_press_ms) {
                m_long_press_triggered = true;
                m_tap_count = 0; // Cancel double tap if it was a long press
                //event = ButtonEvent::LONG_PRESS;
            LOG_DEBUG("Button Long Pressed\n");
            }
        }

        // Logic for Double Tap (after release)
        if (!m_is_pressed && m_tap_count > 0) {
           m_gap_timer += delta_time;
            
            if (m_tap_count >= 2) {
                 // event = ButtonEvent::DOUBLE_TAP;
                LOG_DEBUG("Button Double Tapped\n");
                m_tap_count = 0;
            } 
            else if (m_gap_timer >= m_long_press_ms) {
                // If the gap expires and we only have 1 tap, it was just a single tap
                // (Optional: trigger a SINGLE_TAP event here if needed)
                m_tap_count = 0;
            }
        }

        //return event;

    }

private:
    IPin* m_pin;
    uint8_t m_state;
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