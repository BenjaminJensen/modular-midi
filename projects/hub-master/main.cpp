#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    // Initialize all standard I/O (this enables the USB serial)
    stdio_init_all();

    const uint LED_PIN = 0;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    while (true) {
        gpio_put(LED_PIN, 1);
        printf("LED ON\n");
        sleep_ms(250);

        gpio_put(LED_PIN, 0);
        printf("LED OFF\n");
        sleep_ms(250);
    }
}