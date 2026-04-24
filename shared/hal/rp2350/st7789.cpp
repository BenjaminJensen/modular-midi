#include "st7789.h"
#include "pico/stdlib.h"
#include "AsyncLogger.h"

// A static pointer to the ST7789 instance for the ISR.
// This allows the static ISR to call the instance-specific handler.
static ST7789* dma_irq_instance = nullptr;

const uint16_t phys_width  = 284;
const uint16_t phys_height = 76;

uint16_t disp_row[phys_width];

const uint DEBUG_PIN = 13;

// The static ISR that calls the instance-specific handler
void ST7789::dma_irq_handler_static() {
    if (dma_irq_instance) {
        dma_irq_instance->dma_irq_handler();
    }
}

// Instance-specific ISR
void ST7789::dma_irq_handler() {
    // Check if the interrupt is for our DMA channel
    if (dma_channel_get_irq0_status(dma_channel)) {
        dma_channel_acknowledge_irq0(dma_channel); // Clear the interrupt
        while (spi_is_busy(spi)) tight_loop_contents(); // Wait for SPI to finish
        gpio_put(pin_cs, 1); // Deselect the display
        dma_transfer_in_progress = false; // Mark transfer as complete
        gpio_put(DEBUG_PIN, 0);
    }
}

ST7789::ST7789(spi_inst_t* spi_port, uint sck_pin, uint tx_pin, uint cs_pin, uint dc_pin, uint rst_pin, uint16_t y_offset, uint16_t x_offset)
    : spi(spi_port), pin_sck(sck_pin), pin_tx(tx_pin), pin_cs(cs_pin), pin_dc(dc_pin), pin_rst(rst_pin),
      Y_OFFSET(y_offset), X_OFFSET(x_offset),
      dma_channel(dma_claim_unused_channel(true)), dma_transfer_in_progress(false) {
    dma_irq_instance = this;
}

void ST7789::init_hw() {
    // Initialize SPI at a fast baudrate (62.5 MHz is typical for ST7789 on RP-series)
    spi_init(spi, 10 * 1000 * 1000);
    
    spi_set_format(spi, 16, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);

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

    // Debug pin to monitor DMA activity (optional)
    gpio_init(DEBUG_PIN);
    gpio_set_dir(DEBUG_PIN, GPIO_OUT);
    gpio_put(DEBUG_PIN, 0);

    // Configure DMA for SPI transfers
    // TODO: Assert that dma_channel is valid (not -1) before using it
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_bswap(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(spi, true)); // DREQ for SPI TX
    dma_channel_configure(
        dma_channel,
        &c,
        &spi_get_hw(spi)->dr, // Write to SPI data register
        NULL,                 // Read address is set later
        0,                    // Transfer count is set later
        false                 // Don't start immediately
    );

    // Enable the DMA interrupt on the specified channel
    dma_channel_set_irq0_enabled(dma_channel, true);

    // Configure the ISR and enable it
    irq_set_exclusive_handler(DMA_IRQ_0, ST7789::dma_irq_handler_static);
    irq_set_enabled(DMA_IRQ_0, true);
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
    /*
        Value (Hex) Binary Resulting Layout
        0x70 0111 0000 Original (Top-Left)
        0xB 01011 0000 Lower-Right (Mirrored)
        0xA0 1010 0000 Try this next (Rotated 180°)
        0x60 0110 0000 Try this last (Flipped X/Y)
    */
    set_data_access(0x70);
    
    // Exit sleep mode and prepare to receive pixel data
    set_mode_on();
    sleep_ms(150);

    clear_screen();//(0x07E0); // Clear to green for testing
}

void ST7789::sleep_out() {
    send_cmd(0x11); // Sleep Out
}
void ST7789::update(const uint8_t* data, size_t len, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    // TODO: fix this
    while (dma_transfer_in_progress) { tight_loop_contents(); }

    // Target the physical glass location in RAM
    set_window(X_OFFSET + x1, Y_OFFSET + y1, X_OFFSET + x2, Y_OFFSET + y2);
    set_memory_write();

    send_data(data, len);
}

void ST7789::clear_screen(uint16_t color) {
    while (dma_transfer_in_progress) { tight_loop_contents(); }

    // First block takes 5.6us
    gpio_put(DEBUG_PIN, 1);

    // Target the physical glass location in RAM
    set_window(X_OFFSET, Y_OFFSET, X_OFFSET + phys_width - 1, Y_OFFSET + phys_height - 1);
    set_memory_write();

    gpio_put(DEBUG_PIN, 0);
    
    // Optimization: Write 4 bytes (2 pixels) per loop iteration
    // 12 us optimized
    uint32_t* row_ptr32 = (uint32_t*)disp_row;
    uint32_t two_pixels = (color << 16) | color; // Pack two 16-bit pixels

    // Unrolled 32-bit fill (8 bytes per loop)
    for (int i = 0; i < (phys_width / 2); i += 2) {
        row_ptr32[i]     = two_pixels;
        row_ptr32[i + 1] = two_pixels;
    }

    gpio_put(DEBUG_PIN, 1);
    
    // Blast the data to the display
    for (int y = 0; y < phys_height; y++) {
        send_data((const uint8_t*)disp_row, sizeof(disp_row));
    }
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
    // Wait for any DMA transfer to finish before using SPI directly
    while (dma_transfer_in_progress) {
        tight_loop_contents();
    }
    
    // Temporarily switch to 8-bit SPI for command
    spi_set_format(spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    
    gpio_put(pin_cs, 0);
    gpio_put(pin_dc, 0); // DC low for command
    spi_write_blocking(spi, &cmd, 1);
    gpio_put(pin_cs, 1);
    
    // Restore 16-bit SPI for DMA data
    spi_set_format(spi, 16, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
}

void ST7789::send_data(const uint8_t* data, size_t len) {
    // Wait for any previous DMA transfer to finish.
    while (dma_transfer_in_progress) {
        tight_loop_contents();
    }

    gpio_put(pin_cs, 0);
    gpio_put(pin_dc, 1); // DC high for data

    dma_transfer_in_progress = true;
    // Configure and start the DMA transfer
    dma_channel_set_read_addr(dma_channel, data, false);
    // Since DMA is 16-bit, transfer count is halved
    dma_channel_set_trans_count(dma_channel, len / 2, true); // The 'true' triggers the transfer
}

void ST7789::send_data(uint8_t data) {
    // Wait for any previous DMA transfer to finish.
    while (dma_transfer_in_progress) {
        tight_loop_contents();
    }
    // Temporarily switch to 8-bit SPI for single-byte data
    spi_set_format(spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_put(pin_cs, 0);
    gpio_put(pin_dc, 1); // DC high for data
    spi_write_blocking(spi, &data, 1);
    gpio_put(pin_cs, 1);
    // Restore 16-bit SPI for DMA data
    spi_set_format(spi, 16, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
}

void ST7789::set_window(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    send_cmd(0x2A); // CASET (Column Address Set)

    // These two buffers are static as they are transfered with dma, this i s not good design and needs to be reworked
    static uint16_t x_data[2];
    x_data[0] = (uint16_t)x1;
    x_data[1] = (uint16_t)x2;
    send_data((const uint8_t*)x_data, sizeof(x_data));

    send_cmd(0x2B); // RASET (Row Address Set)
    static uint16_t y_data[2];
    y_data[0] = (uint16_t)y1;
    y_data[1] = (uint16_t)y2;
    send_data((const uint8_t*)y_data, sizeof(y_data));
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