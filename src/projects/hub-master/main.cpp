#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "FreeRTOS.h"
#include "task.h"
#include <array>
#include <cstdio>
#include "logger_instance.h"
#include "display.h"
#include "st7789.h"
#include "spi_dma_bus.h"
#include "output_pin.h"
#include "pico_delay.h"
#include "shared/services/button_service.h"
#include "shared/services/system_service.h"
#include "shared/hal/rp2350/pin.h"
#include "shared/hal/rp2350/freertos_task_runner.h"
#include "shared/hal/rp2350/freertos_event_queue.h"

// Display hw: SPI1 + CS/DC/RST pins matching the panel's wiring on this board.
static SpiDmaBus display_spi(spi1, 10, 11, 10 * 1000 * 1000);
static OutputPin display_cs(9);
static OutputPin display_dc(8);
static OutputPin display_rst(12);
static PicoDelay display_delay;
static ST7789<SpiDmaBus, OutputPin, OutputPin, OutputPin, PicoDelay> st7789(
    display_spi, display_cs, display_dc, display_rst, display_delay);
static Display<ST7789<SpiDmaBus, OutputPin, OutputPin, OutputPin, PicoDelay>> display(st7789);
static FreeRTOSTaskRunner<512> button_runner("ButtonService", 1);
static FreeRTOSEventQueue<8> button_events;
static ButtonService<Pin, RttSink, FreeRTOSTaskRunner<512>, FreeRTOSEventQueue<8>> button_service(button_runner, button_events);

static FreeRTOSTaskRunner<512> system_runner("SystemService", 1);
static SystemService<RttSink, FreeRTOSTaskRunner<512>, FreeRTOSEventQueue<8>> system_service(g_log, system_runner, button_events);

static Pin button_pin(28); // Example pin number
static Button<Pin, RttSink> button(0, &button_pin, g_log, 500); // 500ms long press threshold

void blink_task(void *pvParameters) {
    const uint LED_PIN = 0;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    for(;;) {
        g_log.debug() << "Blink task";
        // printf("Blinking LED\n");
        gpio_put(LED_PIN, true);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_put(LED_PIN, false);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/*
 This is a non FreeRTOS task that will run on Core 1.
 It will be used for updating the display and drawing on screen.
*/
void display_task() {
    const uint LED_PIN = 1;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    display.init();

    for(;;) {
        
        //Logger::log("Display task\n");
  //      busy_wait_ms(5);
        gpio_put(LED_PIN, true);
//        busy_wait_ms(5);
        //display.clear_screen(color); // Clear to red for testing
        display.task(); // This will trigger the LVGL flush callback, which updates the display with the current draw buffer content

        busy_wait_ms(1);
        gpio_put(LED_PIN, false);
    }
}

TaskHandle_t blink_task_handle = nullptr;
// 1. Define the buffers
StaticTask_t xTaskBuffer;
std::array<StackType_t, configMINIMAL_STACK_SIZE> xStack;

int main() {

    button_service.add_button(&button);
    button_service.start();
    system_service.start();

    // This now wakes up the RTT driver instead of USB/UART

    // stdio_init_all();
    
    // Initialize the RTT Logger
    // (This safely configures RTT channels 1 and 2 for Core 0 and Core 1)
    g_log_sink.init();

    // Now you can log freely!

    g_log.debug() << "System starting up...";
    g_log.debug() << "Running on Core: " << get_core_num();
    g_log.debug() << "String test: " << "Hello World!";
    
    // Create the task and pin it strictly to Core 0
    blink_task_handle = xTaskCreateStatic(
        blink_task,           // Function pointer
        "BlinkTask",      // Name
        configMINIMAL_STACK_SIZE,     // Stack depth (in words, not bytes!)
        nullptr,           // Parameters
        1,                 // Priority
        xStack.data(),     // Pointer to the stack array
        &xTaskBuffer       // Pointer to the TCB buffer
    );
    
    // Task for Core 0 (MIDI - High Priority)
    printf("Starting 'display_task' on core 1:\n");

    // Force Core 1 into a known reset state
    multicore_reset_core1();

    // There is a caveat when debugging, as this needs a system reset to be working
    multicore_launch_core1(display_task);

    vTaskStartScheduler();

    while(true);
}