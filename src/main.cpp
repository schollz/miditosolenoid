/**
 * MIDI to Solenoid Controller - Hello World
 *
 * Basic hello world program to verify toolchain and debug setup.
 * Outputs "Hello World" over UART (default pins GP0/GP1).
 */

#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    // Initialize stdio (UART by default)
    stdio_init_all();

    // Small delay to allow USB/UART to initialize
    sleep_ms(2000);

    printf("MIDI to Solenoid Controller\n");
    printf("Pico SDK Version: %d.%d.%d\n",
           PICO_SDK_VERSION_MAJOR,
           PICO_SDK_VERSION_MINOR,
           PICO_SDK_VERSION_REVISION);

    uint32_t count = 0;
    while (true) {
        printf("Hello World! Count: %lu\n", count++);
        sleep_ms(1000);
    }

    return 0;
}
