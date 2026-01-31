/**
 * MIDI to Solenoid Controller - Hello World
 *
 * Basic hello world program to verify toolchain and debug setup.
 * Outputs "Hello World" over UART (default pins GP0/GP1).
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tusb.h"

static absolute_time_t led_off_deadline = nil_time;

// Deadlines for auto-shutoff of each solenoid GPIO (2-9)
static const uint8_t GPIO_BASE = 2;
static const uint8_t GPIO_COUNT = 8;
static absolute_time_t gpio_off_deadline[GPIO_COUNT];

static void handle_midi_packet(const uint8_t packet[4]) {
    const uint8_t cin = packet[0] & 0x0F;
    const uint8_t status = packet[1];
    const uint8_t data1 = packet[2];
    const uint8_t data2 = packet[3];

    (void)cin;

    const uint8_t msg_type = status & 0xF0;
    const uint8_t channel = (status & 0x0F) + 1;

    const uint8_t gpio_index = data1 % GPIO_COUNT;
    const uint8_t gpio_pin = GPIO_BASE + gpio_index;

    // Pulse onboard LED on any MIDI message
    gpio_put(25, 1);
    led_off_deadline = make_timeout_time_ms(100);

    if (msg_type == 0x90 && data2 != 0) {
        // Note On with velocity > 0: turn on GPIO and schedule auto-off
        // Velocity 0 -> 1ms, Velocity 127 -> 100ms (linear scale)
        const uint32_t duration_ms = 1 + (data2 * 99 / 127);
        gpio_put(gpio_pin, 1);
        gpio_off_deadline[gpio_index] = make_timeout_time_ms(duration_ms);
        printf("MIDI Note On  ch=%u note=%u vel=%u dur=%lums\n", channel, data1, data2, duration_ms);
    } else if (msg_type == 0x80 || (msg_type == 0x90 && data2 == 0)) {
        // Note Off: do nothing, let the velocity-based timer handle shutoff
        printf("MIDI Note Off ch=%u note=%u (ignored)\n", channel, data1);
    } else {
        printf("MIDI Msg     ch=%u status=0x%02X d1=%u d2=%u\n",
               channel, status, data1, data2);
    }

}

int main() {
    // Initialize stdio (UART by default)
    stdio_init_all();

    // Initialize GPIOs 2-9 as outputs (solenoid channels) and LED on GPIO25
    for (uint8_t i = 0; i < GPIO_COUNT; ++i) {
        const uint8_t pin = GPIO_BASE + i;
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0);
        gpio_off_deadline[i] = nil_time;
    }
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    gpio_put(25, 0);

    // Init TinyUSB device stack (MIDI)
    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    tusb_init(0, &dev_init);

    // Small delay to allow USB/UART to initialize while servicing USB
    const uint32_t start_ms = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start_ms < 2000) {
        tud_task();
        sleep_ms(1);
    }

    printf("MIDI to Solenoid Controller\n");
    printf("Pico SDK Version: %d.%d.%d\n",
           PICO_SDK_VERSION_MAJOR,
           PICO_SDK_VERSION_MINOR,
           PICO_SDK_VERSION_REVISION);

    uint32_t count = 0;
    uint32_t last_print_ms = to_ms_since_boot(get_absolute_time());
    while (true) {
        tud_task();

        // Drain MIDI packets without blocking; guard against partial reads.
        uint32_t midi_packets = tud_midi_available();
        if (midi_packets) {
            // Cap per loop to keep latency low for other tasks.
            const uint32_t max_packets = 32;
            if (midi_packets > max_packets) {
                midi_packets = max_packets;
            }

            for (uint32_t i = 0; i < midi_packets; ++i) {
                uint8_t packet[4] = {0};
                if (!tud_midi_packet_read(packet)) {
                    break;
                }
                handle_midi_packet(packet);
            }
        }

        // Ensure LED pulse is turned off on time even if no MIDI arrives.
        // Note: handle_midi_packet updates the deadline when MIDI is received.
        if (!is_nil_time(led_off_deadline) &&
            absolute_time_diff_us(get_absolute_time(), led_off_deadline) <= 0) {
            gpio_put(25, 0);
            led_off_deadline = nil_time;
        }

        // Check solenoid GPIO deadlines and turn off expired ones
        for (uint8_t i = 0; i < GPIO_COUNT; ++i) {
            if (!is_nil_time(gpio_off_deadline[i]) &&
                absolute_time_diff_us(get_absolute_time(), gpio_off_deadline[i]) <= 0) {
                gpio_put(GPIO_BASE + i, 0);
                gpio_off_deadline[i] = nil_time;
            }
        }

        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (now_ms - last_print_ms >= 1000) {
            last_print_ms = now_ms;
            printf("Hello World! Count: %lu\n", count++);
        }

        sleep_ms(1);
    }

    return 0;
}
