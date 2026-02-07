# MIDI to Solenoid Controller

RP2040-based controller for driving solenoids. Two modes:

- **Generative (default):** Autonomous pattern engine based on [Mutable Instruments Grids](https://mutable-instruments.net/modules/grids/) by Emilie Gillet. Drives solenoids with algorithmically generated rhythmic patterns.
- **MIDI:** USB MIDI Note On/Off messages trigger solenoid pulses. Velocity controls pulse duration.

## Controls

**User Key (GPIO 23):**
- Short press (generative mode): randomize patterns
- Long press (>1s): toggle between generative and MIDI mode

## Hardware

- Raspberry Pi Pico (RP2040)
- 8 solenoid outputs on GPIO 2-9
- Pico Debug Probe (CMSIS-DAP) for SWD + UART

## Build & Upload

```bash
make            # build
make upload     # flash via debug probe (falls back to USB/UF2)
```

## UART Monitor

115200 baud, 8N1:
```bash
make uart-monitor
```

## MIDI Test

```bash
make midi-list          # list ALSA MIDI devices
make midi-test          # send Note On/Off sequence
NOTE=60 VEL=100 make midi-note-on
NOTE=60 make midi-note-off
```

## Debugging

```bash
make debug-server       # start OpenOCD
make gdb                # connect GDB (loads firmware)
make gdb-attach         # attach without loading
```

## Credits

Generative engine adapted from [Grids](https://github.com/pichenettes/eurorack) by Emilie Gillet (Mutable Instruments), licensed under GPL3.0.
