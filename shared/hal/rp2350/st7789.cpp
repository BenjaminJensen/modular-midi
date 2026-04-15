#include "st7789.h"
#include "pico/stdlib.h"

ST7789::ST7789(spi_inst_t* spi_port, uint sck_pin, uint tx_pin, uint cs_pin, uint dc_pin, uint rst_pin)
    : spi(spi_port), pin_sck(sck_pin), pin_tx(tx_pin), pin_cs(cs_pin), pin_dc(dc_pin), pin_rst(rst_pin),
      dma_channel(dma_claim_unused_channel(true)) {
}

void ST7789::init_hw() {
    // Initialize SPI at a fast baudrate (62.5 MHz is typical for ST7789 on RP-series)
    spi_init(spi, 1 * 1000 * 1000);
    
    spi_set_format(spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);

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

    // Configure DMA for SPI transfers
    // TODO: Assert that dma_channel is valid (not -1) before using it
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, spi_get_dreq(spi, true)); // DREQ for SPI TX
    dma_channel_configure(
        dma_channel,
        &c,
        &spi_get_hw(spi)->dr, // Write to SPI data register
        NULL,                 // Read address is set later
        0,                    // Transfer count is set later
        false                 // Don't start immediately
    );
}
void ST7789::init() {
    init_hw();

    // Perform a hardware reset
    reset();
    sleep_ms(150);

    // Software Init commands for ST7789
    sleep_out(); // Sleep Out
    sleep_ms(150);

    // Set pixel format to 16 bits per pixel (RGB565)
    set_pixel_format(0x05); //  16-bit/pixel (RGB 5-6-5-bit input), 65K-Colors, 3Ah=”05h”

    // Memory Access Control - set rotation and RGB/BGR order as needed
    set_data_access(0x00);
    //send_cmd(0x36); // MADCTL (Memory Data Access Control)
    //send_data(0x00); // Set rotation and RGB/BGR order as needed

    // Optional: Enable display inversion for better contrast (can be toggled based on preference)
    // send_cmd(0x21); // INVON (Display Inversion On)

    // Exit sleep mode and prepare to receive pixel data
    set_mode_on();
    sleep_ms(150);

    //send_cmd(0x13); // NORON (Normal Display Mode On)

    // Clear the screen to black on startup to prevent random noise.
    // We clear the entire 240x320 ST7789 GRAM. This ensures the 76x284
    // physical area is fully cleared regardless of any hardware offsets.
    set_window(0, 0, 240 - 1, 320 - 1);
    set_memory_write();

    uint8_t black_row[240 * 2] = {0}; // 2 bytes per pixel (RGB565)
    for (int y = 0; y < 320; y++) {
        send_data(black_row, sizeof(black_row));
    }
}

void ST7789::sleep_out() {
    send_cmd(0x11); // Sleep Out
}

void ST7789::set_pixel_format(uint8_t format) {
    send_cmd(0x3A); // COLMOD (Color Mode)
    send_data(format);
}

void ST7789::set_data_access(uint8_t format) {
    send_cmd(0x36); // MADCTL (Memory Data Access Control)
    send_data(format);
}

void ST7789::set_mode_on() {
    // NORON (Normal Display Mode On)
    send_cmd(0x13); // Sleep Out
}

void ST7789::set_memory_write() {
    /*
    Page 202 of the ST7789 datasheet:
        -This command is used to transfer data from MCU to frame memory.
        -When this command is accepted, the column register and the page register are reset to the start column/start
        page positions.
        -The start column/start page positions are different in accordance with MADCTL setting.
        -Sending any other command can stop frame write. 
    */
    send_cmd(0x2C); // RAMWR (Memory Write)
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

    // Wait for any previous DMA transfer to finish before starting a new one
    dma_channel_wait_for_finish_blocking(dma_channel);

    // Configure and start the DMA transfer
    dma_channel_set_read_addr(dma_channel, data, false);
    dma_channel_set_trans_count(dma_channel, len, true); // The 'true' triggers the transfer

    // Wait for the DMA transfer to complete
    dma_channel_wait_for_finish_blocking(dma_channel);
    // Wait until SPI is not busy to ensure all data has been sent
    while (spi_is_busy(spi)) tight_loop_contents();

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