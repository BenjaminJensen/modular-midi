#pragma once

#include <cstdint>
#include "shared/hal/delay_concept.h"
#include "shared/hal/display_driver_concept.h"
#include "shared/hal/rp2350/display.h"
#include "shared/render/mailbox.h"

/*
    Owns one physical display's half of the Render Engine (see CONTEXT.md,
    architecture/EVENT_SYSTEM.md, docs/adr/0001): applies whatever the
    Mailbox has pending for display_id and drives the display's own LVGL
    timer/flush handling. display_id is a runtime constructor parameter,
    not a template parameter, so this is the one class reused per display
    (matches Button's id, HAL.md's "one class per peripheral type").

    Doesn't own the core-1 loop itself - see run_render_loop() below. Must
    be given static storage duration, same as every other HAL/Service
    object in this codebase (see main.cpp).
*/
template<DisplayDriver DriverT, uint8_t DisplayCount>
class RenderEngine {
public:
    RenderEngine(Display<DriverT>& display, Mailbox<DisplayCount>& mailbox, uint8_t display_id)
        : m_display(display), m_mailbox(mailbox), m_display_id(display_id) {}

    void init() {
        m_display.init();
    }

    // One iteration's worth of work: apply this display's pending Mailbox
    // entry if it's dirty, then drive LVGL's timer/flush handling regardless.
    // Call this from a tight loop (see run_render_loop below).
    void update() {
        Label label;
        if (m_mailbox.take_if_dirty(m_display_id, label)) {
            m_display.apply_label(label);
        }
        m_display.task();
    }

private:
    Display<DriverT>& m_display;
    Mailbox<DisplayCount>& m_mailbox;
    uint8_t m_display_id;
};

// The core-1 bare loop that drives a single RenderEngine forever. Never
// blocks on the mailbox itself, so it polls engine.update() every iteration
// rather than being woken - see docs/adr/0001. delay is a DelayT& (see
// delay_concept.h) rather than a direct pico-sdk call, so this file has no
// pico-sdk dependency of its own - the concrete PicoDelay is supplied from
// main.cpp, same pattern ST7789 already uses. Extracted here (rather than
// main.cpp) so the only thing left in main.cpp is the one-line trampoline
// multicore_launch_core1's context-free function pointer requires.
template<DisplayDriver DriverT, uint8_t DisplayCount, Delay DelayT>
[[noreturn]] void run_render_loop(RenderEngine<DriverT, DisplayCount>& engine, DelayT& delay) {
    engine.init();

    for (;;) {
        engine.update();
        delay.delay_ms(1);
    }
}
