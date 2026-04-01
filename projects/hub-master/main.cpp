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

int main() {
    // This now wakes up the RTT driver instead of USB/UART
    stdio_init_all();
    
    // Skip stdio_init_all() for a moment to test the pure scheduler
    //xTaskCreate(blink_task, "Blink", 256, NULL, 1, NULL);
    // Create the task and pin it strictly to Core 0
    xTaskCreateAffinitySet(
        blink_task,    // Function
        "Blink",       // Name
        256,           // Stack size
        NULL,          // Parameters
        1,             // Priority
        (1 << 0),      // Affinity Mask: 1 means Core 0 only
        NULL           // Task Handle
    );
    vTaskStartScheduler();
    
    while(1); 
}