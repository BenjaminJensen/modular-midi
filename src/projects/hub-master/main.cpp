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
#include "shared/hal/rp2350/render_engine.h"
#include "shared/render/mailbox.h"

// Display hw: SPI1 + CS/DC/RST pins matching the panel's wiring on this board.
static SpiDmaBus display_spi(spi1, 10, 11, 10 * 1000 * 1000);
static OutputPin display_cs(9);
static OutputPin display_dc(8);
static OutputPin display_rst(12);
static PicoDelay display_delay;
static ST7789<SpiDmaBus, OutputPin, OutputPin, OutputPin, PicoDelay> st7789(
    display_spi, display_cs, display_dc, display_rst, display_delay);
static Display<ST7789<SpiDmaBus, OutputPin, OutputPin, OutputPin, PicoDelay>, RttSink> display(st7789, g_log);

// Cross-core Mailbox for Render Engine label updates (see architecture/EVENT_SYSTEM.md,
// docs/adr/0001). Sized for the full 4-display hardware spec even though only display 0
// is wired up below - display_ids 1-3 simply stay untouched until those displays exist.
static constexpr uint8_t NUM_DISPLAYS = 4;
static Mailbox<NUM_DISPLAYS> render_mailbox;
static RenderEngine<ST7789<SpiDmaBus, OutputPin, OutputPin, OutputPin, PicoDelay>, RttSink, NUM_DISPLAYS>
    render_engine(display, render_mailbox, 0);

static FreeRTOSTaskRunner<512> button_runner("ButtonService", 1);
static FreeRTOSEventQueue<8> button_events;
static ButtonService<Pin, RttSink, FreeRTOSTaskRunner<512>, FreeRTOSEventQueue<8>> button_service(button_runner, button_events);

static FreeRTOSTaskRunner<512> system_runner("SystemService", 1);
static SystemService<RttSink, FreeRTOSTaskRunner<512>, FreeRTOSEventQueue<8>, NUM_DISPLAYS> system_service(
    g_log, system_runner, button_events, render_mailbox);

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

// multicore_launch_core1 takes a context-free void(*)(), so this trampoline
// is the only thing that can't move into render_engine.h - the actual loop
// (run_render_loop) and per-display logic (RenderEngine::update) both live
// there. Reuses display_delay (stateless) rather than declaring a second
// PicoDelay purely for this loop's per-iteration delay.
[[noreturn]] void render_task() {
    run_render_loop(render_engine, display_delay);
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
    printf("Starting 'render_task' on core 1:\n");

    // Force Core 1 into a known reset state
    multicore_reset_core1();

    // There is a caveat when debugging, as this needs a system reset to be working
    multicore_launch_core1(render_task);

    vTaskStartScheduler();

    while(true);
}