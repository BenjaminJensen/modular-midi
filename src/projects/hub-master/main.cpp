#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "FreeRTOS.h"
#include "task.h"
#include <cstdio>
#include "AsyncLogger.h"
#include "display.h"
#include "services/button_service.h"
#include "hal/rp2350/pin.h"
// Statically instantiate the display using the default pins defined in the header

static Display display;
static ButtonService button_service;

static Pin button_pin(28); // Example pin number
static Button button(&button_pin, 500); // 500ms long press threshold

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

    for(;;) {
        
        //Logger::log("Display task\n");
  //      busy_wait_ms(5);
        gpio_put(LED_PIN, 1);
//        busy_wait_ms(5);
        //display.clear_screen(color); // Clear to red for testing
        display.task(); // This will trigger the LVGL flush callback, which updates the display with the current draw buffer content    
        
        busy_wait_ms(1);
        gpio_put(LED_PIN, 0);    
    }
}

TaskHandle_t blink_task_handle = NULL;
// 1. Define the buffers
StaticTask_t xTaskBuffer;
StackType_t xStack[configMINIMAL_STACK_SIZE];

int main() {

    button_service.add_button(&button);
    button_service.start(1);

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
    blink_task_handle = xTaskCreateStatic(
        blink_task,           // Function pointer
        "BlinkTask",      // Name
        configMINIMAL_STACK_SIZE,     // Stack depth (in words, not bytes!)
        NULL,              // Parameters
        1,                 // Priority
        xStack,            // Pointer to the stack array
        &xTaskBuffer       // Pointer to the TCB buffer
    );
    
    // Task for Core 0 (MIDI - High Priority)
    printf("Starting 'display_task' on core 1:\n");

    // Force Core 1 into a known reset state
    multicore_reset_core1();

    // There is a caveat when debugging, as this needs a system reset to be working
    multicore_launch_core1(display_task);

    vTaskStartScheduler();
    
    while(1); 
}