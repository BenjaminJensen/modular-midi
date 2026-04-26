#include "display.h"

// Screen defines
#define SCREEN_WIDTH  284
#define SCREEN_HEIGHT 76
#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT * 2) // 100% of screen, 16-bit


// Static draw buffer in RP2350 RAM
static uint8_t buf1_raw[DRAW_BUF_SIZE];

static struct repeating_timer lvgl_tick_timer;

static bool lv_tick_timer_callback(struct repeating_timer *t) {
    lv_tick_inc(5);
    return true;
}

 Display::Display() {
    display = nullptr;
}

void Display::init() {
    // Initialize the hardware driver for the display
    st7789.set_callback(this);
    st7789.init();
    st7789.display_on();

    display = lvgl_setup();
    add_repeating_timer_ms(5, lv_tick_timer_callback, NULL, &lvgl_tick_timer);
}

void Display::task() {
    //while (true) 
    {
        if (!tmp) {
        /*Change the active screen's background color*/
            lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFFFFF), LV_PART_MAIN);

            /*Create a white label, set its text and align it to the center*/
            lv_obj_t * label = lv_label_create(lv_screen_active());
            lv_label_set_text(label, "Hello world");
            lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0x00ff00), LV_PART_MAIN);
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
            tmp = true;
        }
        lv_timer_handler();
    }
}

lv_display_t * Display::lvgl_setup() {
    lv_init();

    // Create the display object
    lv_display_t * disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);

    // Attach 'this' (the current Display instance) to the LVGL display object
    lv_display_set_user_data(disp, this);

    // Create the Draw Buffer Descriptor (This wraps your raw array)
    static lv_draw_buf_t draw_buf;
    lv_draw_buf_init(&draw_buf, SCREEN_WIDTH, SCREEN_HEIGHT / 10, LV_COLOR_FORMAT_RGB565, 0, buf1_raw, sizeof(buf1_raw));

    // Assign the descriptor to the display
    // Note: v9 uses only 3 arguments for this specific function now
    lv_display_set_draw_buffers(disp, &draw_buf, NULL); 
    
    lv_display_set_flush_cb(disp, Display::lv_flush_wrapper);
    return disp;
}

void Display::lv_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    // Calculate the number of pixels in the area
    size_t pixel_count = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    // Update the display with the pixel data for the specified area
    st7789.update(px_map, pixel_count * 2, area->x1, area->y1, area->x2, area->y2);
}
void Display::lv_flush_wrapper(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    // Get instance pointer from user data
    Display* instance = (Display*)lv_display_get_user_data(disp);
    instance->lv_flush(disp, area, px_map);

}