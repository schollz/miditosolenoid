/**
 * MIDI to Solenoid Controller
 *
 * Two modes:
 *   1. Generative mode (default): Grids pattern engine drives solenoids autonomously
 *   2. MIDI mode: USB MIDI Note On/Off -> solenoid pulses
 *
 * User Key (GPIO 23, active-low):
 *   - Short press in Generative mode: randomize patterns
 *   - Long press (>1s): toggle between Generative and MIDI mode
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "tusb.h"

#include "generative/generative_controller.h"

// Set to 1 for detailed generative mode UART logging, 0 for quiet
#define GEN_VERBOSE 1

// GPIO assignments
static const uint8_t GPIO_BASE = 2;
static const uint8_t GPIO_COUNT = 8;
static const uint8_t GPIO_LED = 25;
static const uint8_t GPIO_USER_KEY = 23;

// Button debounce/long-press timing (ms)
static const uint32_t DEBOUNCE_MS = 50;
static const uint32_t LONG_PRESS_MS = 1000;

// Solenoid auto-off deadlines
static absolute_time_t led_off_deadline = nil_time;
static absolute_time_t gpio_off_deadline[GPIO_COUNT];

// Mode state
static bool generative_mode = true;
static generative::GenerativeController gen_controller;

// Button state
static bool btn_last_raw = false;       // last raw GPIO reading (true = pressed)
static bool btn_stable = false;         // debounced state
static uint32_t btn_change_ms = 0;      // timestamp of last raw change
static uint32_t btn_press_start_ms = 0; // when the current press began
static bool btn_handled = false;        // has this press been handled already?

// Default BPM in tenths (120.0 BPM)
static const uint32_t DEFAULT_BPM_TENTHS = 1200;

static void all_solenoids_off() {
    for (uint8_t i = 0; i < GPIO_COUNT; ++i) {
        gpio_put(GPIO_BASE + i, 0);
        gpio_off_deadline[i] = nil_time;
    }
}

static void handle_midi_packet(const uint8_t packet[4]) {
    const uint8_t status = packet[1];
    const uint8_t data1 = packet[2];
    const uint8_t data2 = packet[3];

    const uint8_t msg_type = status & 0xF0;
    const uint8_t channel = (status & 0x0F) + 1;

    const uint8_t gpio_index = data1 % GPIO_COUNT;
    const uint8_t gpio_pin = GPIO_BASE + gpio_index;

    // Pulse onboard LED on any MIDI message
    gpio_put(GPIO_LED, 1);
    led_off_deadline = make_timeout_time_ms(100);

    if (msg_type == 0x90 && data2 != 0) {
        const uint32_t duration_ms = 1 + (data2 * 99 / 127);
        gpio_put(gpio_pin, 1);
        gpio_off_deadline[gpio_index] = make_timeout_time_ms(duration_ms);
        printf("MIDI Note On  ch=%u note=%u vel=%u dur=%lums\n", channel, data1, data2, duration_ms);
    } else if (msg_type == 0x80 || (msg_type == 0x90 && data2 == 0)) {
        printf("MIDI Note Off ch=%u note=%u (ignored)\n", channel, data1);
    } else {
        printf("MIDI Msg     ch=%u status=0x%02X d1=%u d2=%u\n",
               channel, status, data1, data2);
    }
}

// Process button: returns action
// 0 = no action, 1 = short press, 2 = long press
static uint8_t process_button(uint32_t now_ms) {
    bool raw = !gpio_get(GPIO_USER_KEY);  // active-low

    // Detect raw change for debounce
    if (raw != btn_last_raw) {
        btn_last_raw = raw;
        btn_change_ms = now_ms;
    }

    // Only update stable state after debounce period
    if ((now_ms - btn_change_ms) < DEBOUNCE_MS) {
        return 0;
    }

    bool prev_stable = btn_stable;
    btn_stable = raw;

    // Rising edge (button just pressed)
    if (btn_stable && !prev_stable) {
        btn_press_start_ms = now_ms;
        btn_handled = false;
        return 0;
    }

    // Button held: check for long press
    if (btn_stable && !btn_handled) {
        if ((now_ms - btn_press_start_ms) >= LONG_PRESS_MS) {
            btn_handled = true;
            return 2;  // long press
        }
    }

    // Falling edge (button released)
    if (!btn_stable && prev_stable && !btn_handled) {
        btn_handled = true;
        return 1;  // short press
    }

    return 0;
}

int main() {
    stdio_init_all();

    // Initialize solenoid GPIOs (2-9)
    for (uint8_t i = 0; i < GPIO_COUNT; ++i) {
        const uint8_t pin = GPIO_BASE + i;
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0);
        gpio_off_deadline[i] = nil_time;
    }

    // LED
    gpio_init(GPIO_LED);
    gpio_set_dir(GPIO_LED, GPIO_OUT);
    gpio_put(GPIO_LED, 0);

    // User Key (GPIO 23) - input with pull-up
    gpio_init(GPIO_USER_KEY);
    gpio_set_dir(GPIO_USER_KEY, GPIO_IN);
    gpio_pull_up(GPIO_USER_KEY);

    // Init TinyUSB
    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    tusb_init(0, &dev_init);

    // Startup delay (service USB while waiting)
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
    printf("Modes: Generative (default) | MIDI (User Key long-press)\n");

    // Initialize generative mode at startup
    {
        uint32_t seed = time_us_32();
        gen_controller.Init(seed, DEFAULT_BPM_TENTHS);
#if GEN_VERBOSE
        gen_controller.SetVerbose(true);
#endif
        printf("=== GENERATIVE MODE ON ===\n");
        gen_controller.PrintPatterns();
        stdio_flush();
        sleep_ms(200);  // let UART drain before triggers start
    }

    uint32_t count = 0;
    uint32_t last_print_ms = to_ms_since_boot(get_absolute_time());

    while (true) {
        tud_task();

        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        // --- Button handling ---
        uint8_t btn_action = process_button(now_ms);

        if (btn_action == 1) {  // short press
            if (generative_mode) {
                printf("[KEY] short press - randomize\n");
                gen_controller.Randomize();
                printf("=== PATTERNS RANDOMIZED ===\n");
            }
        } else if (btn_action == 2) {  // long press
            generative_mode = !generative_mode;
            all_solenoids_off();
            if (generative_mode) {
                uint32_t seed = time_us_32();
                gen_controller.Init(seed, DEFAULT_BPM_TENTHS);
#if GEN_VERBOSE
                gen_controller.SetVerbose(true);
#endif
                printf("=== GENERATIVE MODE ===\n");
                gen_controller.PrintPatterns();
            } else {
                gpio_put(GPIO_LED, 0);
                led_off_deadline = nil_time;
                printf("=== MIDI MODE ===\n");
            }
        }

        // --- Mode-specific processing ---
        if (generative_mode) {
            // Tick the generative engine (1ms resolution)
            generative::FireEvent event = gen_controller.Tick();

            // Fire solenoids from generative triggers
            if (event.gpio_mask) {
                for (uint8_t i = 0; i < GPIO_COUNT; ++i) {
                    if (event.gpio_mask & (1 << i)) {
                        gpio_put(GPIO_BASE + i, 1);
                        gpio_off_deadline[i] = make_timeout_time_ms(event.duration_ms[i]);
                    }
                }
            }

            // LED beat indicator: blink on beat (every 8 steps)
            uint8_t step = gen_controller.step();
            if ((step & 0x07) == 0) {
                gpio_put(GPIO_LED, 1);
                led_off_deadline = make_timeout_time_ms(50);
            }

            // Silently drain MIDI to keep USB healthy
            uint32_t midi_packets = tud_midi_available();
            if (midi_packets) {
                if (midi_packets > 32) midi_packets = 32;
                for (uint32_t i = 0; i < midi_packets; ++i) {
                    uint8_t packet[4] = {0};
                    if (!tud_midi_packet_read(packet)) break;
                }
            }
        } else {
            // MIDI mode: process incoming MIDI packets
            uint32_t midi_packets = tud_midi_available();
            if (midi_packets) {
                const uint32_t max_packets = 32;
                if (midi_packets > max_packets) {
                    midi_packets = max_packets;
                }
                for (uint32_t i = 0; i < midi_packets; ++i) {
                    uint8_t packet[4] = {0};
                    if (!tud_midi_packet_read(packet)) break;
                    handle_midi_packet(packet);
                }
            }
        }

        // --- Shared: LED and solenoid deadline checks ---
        if (!is_nil_time(led_off_deadline) &&
            absolute_time_diff_us(get_absolute_time(), led_off_deadline) <= 0) {
            gpio_put(GPIO_LED, 0);
            led_off_deadline = nil_time;
        }

        for (uint8_t i = 0; i < GPIO_COUNT; ++i) {
            if (!is_nil_time(gpio_off_deadline[i]) &&
                absolute_time_diff_us(get_absolute_time(), gpio_off_deadline[i]) <= 0) {
                gpio_put(GPIO_BASE + i, 0);
                gpio_off_deadline[i] = nil_time;
            }
        }

        // Heartbeat (MIDI mode only)
        if (!generative_mode) {
            if (now_ms - last_print_ms >= 1000) {
                last_print_ms = now_ms;
                printf("Hello World! Count: %lu\n", count++);
            }
        }

        sleep_ms(1);
    }

    return 0;
}
