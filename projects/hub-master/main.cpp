#include "pico/stdlib.h"
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

void display_task(void *pvParameters) {

    for(;;) {
        printf("Displaying message\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main() {
    // This now wakes up the RTT driver instead of USB/UART
    stdio_init_all();
    
    // Create the task and pin it strictly to Core 0
    xTaskCreate(blink_task, "Blink", 256, NULL, 1, NULL);
    
    // Task for Core 0 (MIDI - High Priority)
 
    vTaskStartScheduler();
    
    while(1); 
}