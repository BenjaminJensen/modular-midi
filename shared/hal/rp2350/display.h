#include "lvgl.h"
#include "pico/stdlib.h"

class Display {
public:
    Display();
    void init();
     
private:
    lv_display_t * display_setup();
    lv_display_t * display;
};