#include "lvgl.h"
#include "pico/stdlib.h"
#include "st7789.h"

class Display {
public:
    Display();
    void init();
    void clear_screen(uint16_t color = 0x0000) {
        st7789.clear_screen(color);
    }
     
private:
    lv_display_t * lvgl_setup();
    lv_display_t * display;
    ST7789 st7789;
};