#include "display.h"
#include "st7789.h"


// Screen defines
#define SCREEN_WIDTH  76
#define SCREEN_HEIGHT 284
#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * 2) // 10% of screen, 16-bit


// Static draw buffer in RP2350 RAM
static uint8_t buf1_raw[DRAW_BUF_SIZE];

 Display::Display() {
    display = nullptr;
}

void Display::init() {
    display = display_setup();
}

lv_display_t * Display::display_setup() {
    lv_init();

    // Create the display object
    lv_display_t * disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);

    // Create the Draw Buffer Descriptor (This wraps your raw array)
    static lv_draw_buf_t draw_buf;
    lv_draw_buf_init(&draw_buf, SCREEN_WIDTH, SCREEN_HEIGHT / 10, LV_COLOR_FORMAT_RGB565, 0, buf1_raw, sizeof(buf1_raw));

    // Assign the descriptor to the display
    // Note: v9 uses only 3 arguments for this specific function now
    lv_display_set_draw_buffers(disp, &draw_buf, NULL); 
    
    //lv_display_set_flush_cb(disp, my_display_flush);
    return disp;
}


