#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "FreeRTOS.h"
#include "task.h"
#include <cstdio>
#include "st7789.h"
#include "AsyncLogger.h"
#include "display.h"

// Statically instantiate the display using the default pins defined in the header

static Display display;

uint16_t add_ten_to_channels(uint16_t color) {
    // 1. Unpack the channels
    uint8_t r = (color >> 11) & 0x1F; // Extract 5 bits Red
    uint8_t g = (color >> 5)  & 0x3F; // Extract 6 bits Green
    uint8_t b =  color        & 0x1F; // Extract 5 bits Blue

    // 2. Add 10 and clamp (so they don't wrap around)
    r = (r + 10 > 31) ? 31 : r + 10;
    g = (g + 10 > 63) ? 63 : g + 10;
    b = (b + 10 > 31) ? 31 : b + 10;

    // 3. Repack into RGB565
    return (r << 11) | (g << 5) | b;
}

uint16_t get_rainbow_color(uint8_t pos) {
    uint8_t r, g, b;
    uint8_t phase = pos / 43; // 256 / 6 sectors ≈ 42.6
    uint8_t f = (pos % 43) * 6; // Fractional part scaled to 0-255

    switch (phase) {
        case 0: r = 255; g = f;   b = 0;   break; // Red to Yellow
        case 1: r = 255 - f; g = 255; b = 0;   break; // Yellow to Green
        case 2: r = 0;   g = 255; b = f;   break; // Green to Cyan
        case 3: r = 0;   g = 255 - f; b = 255; break; // Cyan to Blue
        case 4: r = f;   g = 0;   b = 255; break; // Blue to Magenta
        default: r = 255; g = 0;   b = 255 - f; break; // Magenta to Red
    }

    // Pack into RGB565: Red(5bit), Green(6bit), Blue(5bit)
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t get_cycle_color(uint16_t angle) {
    angle = angle % 360; // Keep it within 0-359 degrees
    
    float f_angle = angle / 60.0f;
    int sector = (int)f_angle;
    float fractional = f_angle - sector;

    // We assume Saturation (S) = 1.0 and Value (V) = 1.0 for bright colors
    uint8_t p = 0;               // V * (1 - S)
    uint8_t q = (1.0 - fractional) * 31; 
    uint8_t t = fractional * 31;
    uint8_t v = 31;              // Max 5-bit value

    uint8_t r, g, b;

    switch(sector) {
        case 0: r = v; g = t*2; b = p; break; // Red to Yellow (Green is 6-bit, so *2)
        case 1: r = q; g = v*2; b = p; break; // Yellow to Green
        case 2: r = p; g = v*2; b = t; break; // Green to Cyan
        case 3: r = p; g = q*2; b = v; break; // Cyan to Blue
        case 4: r = t; g = p;   b = v; break; // Blue to Magenta
        default: r = v; g = p;  b = q; break; // Magenta to Red
    }

    return (r << 11) | (g << 5) | b;
}

void blink_task(void *pvParameters) {
    const uint LED_PIN = 0;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    for(;;) {
        Logger::log("Blink task\n");
        // printf("Blinking LED\n");
        gpio_put(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_put(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/*
 This is a non FreeRTOS task that will run on Core 1. 
 It will be used for updating the display and drawing on screen.
*/
void display_task(void) {
    const uint LED_PIN = 1;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    display.init();
    

    uint16_t color = 0x0000; // Blue in RGB565
    uint8_t pos = 0;
    for(;;) {
        
        Logger::log("Display task\n");
        gpio_put(LED_PIN, 1);
        busy_wait_ms(25);
        gpio_put(LED_PIN, 0);
        busy_wait_ms(25);
        color = get_rainbow_color(pos);
        display.clear_screen(color); // Clear to red for testing
        pos++;
        
    }
}

int main() {
    // This now wakes up the RTT driver instead of USB/UART

    // stdio_init_all();
    
    // Initialize the RTT Logger 
    // (This safely configures RTT channels 1 and 2 for Core 0 and Core 1)
    Logger::init();

    // Now you can log freely!
    Logger::log("System starting up...\n");
    Logger::log("Running on Core: %d\n", get_core_num());
    Logger::log("String test: %s\n", "Hello World!");
    
    // Create the task and pin it strictly to Core 0
    xTaskCreate(blink_task, "Blink", 256, NULL, 1, NULL);
    
    // Task for Core 0 (MIDI - High Priority)
    printf("Starting 'display_task' on core 1:\n");

    // Force Core 1 into a known reset state
    multicore_reset_core1();

    // There is a caveat when debugging, as this needs a system reset to be working
    multicore_launch_core1(display_task);

    vTaskStartScheduler();
    
    while(1); 
}