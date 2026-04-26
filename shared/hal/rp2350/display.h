#include "lvgl.h"
#include "pico/stdlib.h"
#include "st7789.h"
#include "dma_callback.h"

class Display : public DMA_Callback {
public:
    Display();
    void init();

    void task() ;
    // This is the actual function that runs
    void on_dma_complete() override {
        lv_display_flush_ready(this->display);
    }
     
private:
    lv_display_t * lvgl_setup();
    lv_display_t * display;
    ST7789 st7789;
    bool tmp = false;
    static void lv_flush_wrapper(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
    static void dma_complete_callback(void* user_data);
    void lv_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

};