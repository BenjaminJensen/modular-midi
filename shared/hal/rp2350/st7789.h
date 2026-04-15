#pragma once

#include <cstdint>
#include "hardware/spi.h"
#include "hardware/irq.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

class ST7789 {
public:
    // Constructor allows pin override. Defaults assume typical SPI1 pins on a Pico/RP2350
    ST7789(spi_inst_t* spi_port = spi1, uint sck_pin = 10, uint tx_pin = 11, uint cs_pin = 9, uint dc_pin = 8, uint rst_pin = 12);
    
    // External API
    void init();
    void display_on();
    
    // Non-static flush function for the class instance
    //void flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

    // Static wrapper to register with LVGL v9
    //static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

private:
    // Hardware Configuration
    spi_inst_t* spi;
    uint pin_sck;
    uint pin_tx;
    uint pin_cs;
    uint pin_dc;
    uint pin_rst;
    int dma_channel;
    volatile bool dma_transfer_in_progress;

    // Internal helpers
    void init_hw();
    void reset();
    void sleep_out();
    void set_pixel_format(uint8_t format);
    void set_mode_on();
    void set_memory_write();
    void set_data_access(uint8_t format);
    void send_cmd(uint8_t cmd);
    void send_data(const uint8_t* data, size_t len);
    void send_data(uint8_t data);
    
    // Sets the column and row address window for ST7789
    void set_window(int32_t x1, int32_t y1, int32_t x2, int32_t y2);

private:
    // DMA interrupt handler
    void dma_irq_handler();
    static void dma_irq_handler_static();
};