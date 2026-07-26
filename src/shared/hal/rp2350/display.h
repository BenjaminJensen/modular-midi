#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "lvgl.h"
#include "pico/stdlib.h"
#include "shared/hal/display_driver_concept.h"
#include "shared/hal/log_sink_concept.h"
#include "shared/logger.h"
#include "shared/render/label.h"

/*
    LVGL glue for one physical display. Templated on the hardware driver
    (anything satisfying DisplayDriver - ST7789 today), so this class knows
    nothing about SPI/DMA/GPIO and the driver knows nothing about LVGL; the
    only coupling is the small DisplayDriver concept.

    LVGL itself (lv_init(), the tick timer) is process-global state, not
    per-display, so it's set up once regardless of how many Display instances
    exist - see ensure_lvgl_globals_initialized(). That guard only works
    correctly if every physical display instantiates the *same* Display<DriverT>
    type (same DriverT), which is the expected setup (identical panels).

    Also templated on SinkT (LogSink), matching the rest of the codebase's
    DI-via-Logger<SinkT> pattern (Button, ButtonService, SystemService), so
    flush/timing diagnostics go through the same injected sink as everything
    else instead of a global.

    Must be given static storage duration - construct only as a `static`
    object in the glue layer (main.cpp), same as every other HAL/Service
    object in this codebase. operator new is deleted below to block heap
    use, matching the project's no-heap rule; there's no equivalent
    compile-time guard against stack use, so this is enforced by convention
    and code review, not the type system.
*/
template<DisplayDriver DriverT, LogSink SinkT>
class Display {
public:
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;

    Display(DriverT& driver, Logger<SinkT>& logger) : m_driver(driver), m_logger(&logger) {}

    void init() {
        m_driver.init();
        m_driver.display_on();

        ensure_lvgl_globals_initialized();

        m_lv_display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_display_set_user_data(m_lv_display, this);

        lv_draw_buf_init(&m_draw_buf, SCREEN_WIDTH, CHUNK_HEIGHT,
                          LV_COLOR_FORMAT_RGB565, 0, m_draw_buf_raw.data(), m_draw_buf_raw.size());
        lv_display_set_draw_buffers(m_lv_display, &m_draw_buf, nullptr);

        lv_display_set_flush_cb(m_lv_display, &Display::flush_cb_trampoline);

        // Explicit black background: with LV_USE_THEME_DEFAULT off (lv_conf.h),
        // LVGL's base style leaves the screen white, which makes
        // ColorPalette::Default (white, see to_lv_color() below) invisible.
        lv_obj_t* screen = lv_display_get_screen_active(m_lv_display);
        lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

        m_label = lv_label_create(screen);
        lv_obj_center(m_label);
    }

    // Drives LVGL's own timer handler. Call this from a tight loop (see
    // render_task in main.cpp). flush() (below) blocks until each chunk's
    // DMA transfer finishes before returning, so by the time this call
    // returns there is never a redraw left mid-flight - see flush()'s
    // comment for why that's required, not just simpler.
    void task() {
        lv_timer_handler();
    }

    // Applies a Mailbox entry (see architecture/EVENT_SYSTEM.md's Render Engine
    // design) to this display's Label widget. Called by render_task only when
    // Mailbox::take_if_dirty() reports a change - not every loop iteration.
    void apply_label(const Label& label) {
        lv_label_set_text(m_label, label.text_cstr());
        lv_obj_set_style_text_color(m_label, to_lv_color(label.color), 0);
        lv_obj_set_style_text_font(m_label, to_lv_font(label.font), 0);
    }

private:
    static lv_color_t to_lv_color(ColorPalette color) {
        switch (color) {
            case ColorPalette::Active:  return lv_palette_main(LV_PALETTE_GREEN);
            case ColorPalette::Default: return lv_color_white();
        }
        return lv_color_white();
    }

    static const lv_font_t* to_lv_font(FontSize font) {
        switch (font) {
            case FontSize::Small: return &lv_font_montserrat_20;
            case FontSize::Large: return &lv_font_montserrat_42;
        }
        return &lv_font_montserrat_42;
    }

    static constexpr uint16_t SCREEN_WIDTH = 284;
    static constexpr uint16_t SCREEN_HEIGHT = 76;

    // Half-screen-height buffer: a full-screen redraw needs at most 2 chunks
    // rather than the ~11 a smaller buffer would need. A single buffer is
    // sufficient - see flush()'s comment for why double buffering wouldn't
    // actually help here, even though it looks like the obvious fix.
    static constexpr uint16_t CHUNK_HEIGHT = SCREEN_HEIGHT / 2;
    static constexpr size_t CHUNK_BUF_SIZE = static_cast<size_t>(SCREEN_WIDTH) * CHUNK_HEIGHT * 2;

    DriverT& m_driver;
    Logger<SinkT>* m_logger;
    lv_display_t* m_lv_display = nullptr;
    lv_draw_buf_t m_draw_buf{};
    std::array<uint8_t, CHUNK_BUF_SIZE> m_draw_buf_raw{};
    lv_obj_t* m_label = nullptr;

    static void ensure_lvgl_globals_initialized() {
        static bool initialized = false;
        if (initialized) return;
        initialized = true;

        lv_init();

        static struct repeating_timer tick_timer;
        add_repeating_timer_ms(5, &Display::lv_tick_timer_callback, nullptr, &tick_timer);
    }

    static bool lv_tick_timer_callback(struct repeating_timer* /*t*/) {
        lv_tick_inc(5);
        return true;
    }

    void flush(const lv_area_t* area, uint8_t* px_map) {
        const size_t pixel_count = static_cast<size_t>(lv_area_get_width(area)) *
                                    static_cast<size_t>(lv_area_get_height(area));
        m_logger->debug() << "display: flush x1=" << area->x1 << " y1=" << area->y1
                           << " x2=" << area->x2 << " y2=" << area->y2;
        m_driver.update(reinterpret_cast<const uint16_t*>(px_map), pixel_count,
                         area->x1, area->y1, area->x2, area->y2);

        // Blocks until the DMA transfer physically completes, then signals
        // LVGL from right here, before returning - rather than the more
        // "async-looking" pattern of returning immediately and signaling
        // readiness from a later task() poll. That async pattern only works
        // if lv_display_flush_ready() can be called from an interrupt able
        // to preempt LVGL's own wait_for_flushing() spin loop (lv_refr.c) -
        // which runs synchronously, on this same core/thread, for every
        // chunk after the first in a multi-chunk redraw. We deliberately
        // don't use interrupts here (see CLAUDE.md's Display section), so
        // that spin can only ever be satisfied by code running later on this
        // same call stack - which can never happen while task() is still
        // inside this very call. Observed on target as: first chunk flushes
        // fine, second chunk deadlocks forever inside LVGL. Blocking here
        // avoids that entirely: disp->flushing is already clear by the time
        // LVGL checks it for the next chunk, no matter how many chunks a
        // redraw needs.
        while (m_driver.is_busy()) {
        }
        m_logger->debug() << "display: flush complete";
        lv_display_flush_ready(m_lv_display);
    }

    static void flush_cb_trampoline(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
        static_cast<Display*>(lv_display_get_user_data(disp))->flush(area, px_map);
    }
};
