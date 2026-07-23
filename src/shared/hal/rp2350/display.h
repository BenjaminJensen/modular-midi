#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "lvgl.h"
#include "pico/stdlib.h"
#include "shared/hal/display_driver_concept.h"
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

    Must be given static storage duration - construct only as a `static`
    object in the glue layer (main.cpp), same as every other HAL/Service
    object in this codebase. operator new is deleted below to block heap
    use, matching the project's no-heap rule; there's no equivalent
    compile-time guard against stack use, so this is enforced by convention
    and code review, not the type system.
*/
template<DisplayDriver DriverT>
class Display {
public:
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;

    explicit Display(DriverT& driver) : m_driver(driver) {}

    void init() {
        m_driver.init();
        m_driver.display_on();

        ensure_lvgl_globals_initialized();

        m_lv_display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_display_set_user_data(m_lv_display, this);

        lv_draw_buf_init(&m_draw_buf, SCREEN_WIDTH, SCREEN_HEIGHT / 10,
                          LV_COLOR_FORMAT_RGB565, 0, m_draw_buf_raw.data(), m_draw_buf_raw.size());
        lv_display_set_draw_buffers(m_lv_display, &m_draw_buf, nullptr);

        lv_display_set_flush_cb(m_lv_display, &Display::flush_cb_trampoline);

        m_label = lv_label_create(lv_display_get_screen_active(m_lv_display));
        lv_obj_center(m_label);
    }

    // Drives LVGL's own timer handler and, once a previously started flush's
    // DMA transfer has finished, tells LVGL it can start the next one. Call
    // this from a tight loop (see render_task in main.cpp).
    void task() {
        if (m_flush_pending && !m_driver.is_busy()) {
            m_flush_pending = false;
            lv_display_flush_ready(m_lv_display);
        }
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
    static constexpr size_t DRAW_BUF_SIZE = static_cast<size_t>(SCREEN_WIDTH) * SCREEN_HEIGHT * 2;

    DriverT& m_driver;
    lv_display_t* m_lv_display = nullptr;
    lv_draw_buf_t m_draw_buf{};
    std::array<uint8_t, DRAW_BUF_SIZE> m_draw_buf_raw{};
    bool m_flush_pending = false;
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
        m_flush_pending = true;
        m_driver.update(reinterpret_cast<const uint16_t*>(px_map), pixel_count,
                         area->x1, area->y1, area->x2, area->y2);
    }

    static void flush_cb_trampoline(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
        static_cast<Display*>(lv_display_get_user_data(disp))->flush(area, px_map);
    }
};
