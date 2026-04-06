#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "FreeRTOS.h"
#include "task.h"
#include <cstdio>

void blink_task(void *pvParameters) {
    const uint LED_PIN = 0;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    for(;;) {
        printf("Blinking LED\n");
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

    for(;;) {
        gpio_put(LED_PIN, 1);
        busy_wait_ms(250);
        gpio_put(LED_PIN, 0);
        busy_wait_ms(250);
    }
}

int main() {
    // This now wakes up the RTT driver instead of USB/UART
    stdio_init_all();
    
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