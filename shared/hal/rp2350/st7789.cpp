#include "st7789.h"
#include "pico/stdlib.h"

ST7789::ST7789(spi_inst_t* spi_port, uint sck_pin, uint tx_pin, uint cs_pin, uint dc_pin, uint rst_pin)
    : spi(spi_port), pin_sck(sck_pin), pin_tx(tx_pin), pin_cs(cs_pin), pin_dc(dc_pin), pin_rst(rst_pin) {
}

void ST7789::init() {
    // 1. Initialize SPI at a fast baudrate (62.5 MHz is typical for ST7789 on RP-series)
    spi_init(spi, 10 * 1000 * 1000);
    
    // 2. Set Pin functions for SPI SCK and MOSI (TX)
    gpio_set_function(pin_sck, GPIO_FUNC_SPI);
    gpio_set_function(pin_tx, GPIO_FUNC_SPI);
    
    // 3. Initialize Control Pins as standard GPIO outputs
    gpio_init(pin_cs);
    gpio_set_dir(pin_cs, GPIO_OUT);
    gpio_put(pin_cs, 1); // Deselect

    gpio_init(pin_dc);
    gpio_set_dir(pin_dc, GPIO_OUT);
    gpio_put(pin_dc, 1);

    gpio_init(pin_rst);
    gpio_set_dir(pin_rst, GPIO_OUT);
    gpio_put(pin_rst, 1);

    // 4. Perform a hardware reset
    reset();

    // 5. Software Init commands for ST7789
    send_cmd(0x11); // Sleep Out
    sleep_ms(120);

    // --- Add additional required ST7789 configuration commands here ---
    // e.g., Color format (0x3A), Memory data access control (0x36), etc.
}

void ST7789::reset() {
    gpio_put(pin_rst, 0);
    sleep_ms(100);
    gpio_put(pin_rst, 1);
    sleep_ms(100);
}

void ST7789::send_cmd(uint8_t cmd) {
    gpio_put(pin_cs, 0);
    gpio_put(pin_dc, 0); // DC low for command
    spi_write_blocking(spi, &cmd, 1);
    gpio_put(pin_cs, 1);
}

void ST7789::send_data(const uint8_t* data, size_t len) {
    gpio_put(pin_cs, 0);
    gpio_put(pin_dc, 1); // DC high for data
    spi_write_blocking(spi, data, len);
    gpio_put(pin_cs, 1);
}

void ST7789::send_data(uint8_t data) {
    send_data(&data, 1);
}

void ST7789::set_window(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    send_cmd(0x2A); // CASET (Column Address Set)
    uint8_t x_data[] = { (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF), (uint8_t)(x2 >> 8), (uint8_t)(x2 & 0xFF) };
    send_data(x_data, sizeof(x_data));

    send_cmd(0x2B); // RASET (Row Address Set)
    uint8_t y_data[] = { (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF), (uint8_t)(y2 >> 8), (uint8_t)(y2 & 0xFF) };
    send_data(y_data, sizeof(y_data));
}

void ST7789::display_on() {
    send_cmd(0x29); // DISPON (Display On)
    sleep_ms(100);
}
/*
void ST7789::flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    set_window(area->x1, area->y1, area->x2, area->y2);
    send_cmd(0x2C); // RAMWR (Memory Write)
    
    uint32_t pixel_count = lv_area_get_width(area) * lv_area_get_height(area);
    send_data(px_map, pixel_count * 2); // Assuming RGB565 (2 bytes per pixel)

    lv_display_flush_ready(disp); // Vital for LVGL v9
}

void ST7789::flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    ST7789* driver = static_cast<ST7789*>(lv_display_get_user_data(disp));
    if (driver) driver->flush(disp, area, px_map);
}
    */