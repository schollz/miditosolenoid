# MIDI to Solenoid Controller - Project Notes

## Project Overview
RP2040-based MIDI device for controlling solenoids.

## Hardware Setup
- **Target**: RP2040 microcontroller
- **Debug Interface**: Pico Debug Probe (CMSIS-DAP)
- **UART**: Connected via debug probe (GP0=TX, GP1=RX)
- **Debug**: SWD via debug probe

## Dependencies
All dependencies are local in `deps/` folder:
- Pico SDK v2.0.0 (`deps/pico-sdk/`)

## Build System
- CMake-based build configured to use local Pico SDK
- Makefile wrapper for common operations

### Build Commands
```bash
make              # Build project
make clean        # Clean build folder only
make cleanall     # Clean build folder AND deps folder
make upload       # Upload via debug probe
make debug-server # Start OpenOCD (terminal 1)
make gdb          # Connect GDB (terminal 2)
make uart-monitor # Monitor UART output
```

## Current Status
- [x] Project structure created
- [x] Pico SDK v2.0.0 installed locally
- [x] CMakeLists.txt configured for local SDK
- [x] Makefile with build/upload/debug targets
- [x] Hello World program (UART output)
- [x] Verify build compiles
- [x] Verify debug probe connection
- [x] Verify UART output
- [ ] MIDI implementation (future)

## Pin Assignments (Planned)
- GP0: UART TX (debug)
- GP1: UART RX (debug)
- TBD: Solenoid outputs
- TBD: MIDI input

## Development Log

### Session 1
- Initialized project structure
- Cloned Pico SDK v2.0.0 to deps/pico-sdk
- Created CMakeLists.txt with local SDK path
- Created Makefile with build, upload, and debug targets
- Created hello world main.cpp with UART output

### Session 2
- Built firmware via `make gdb` and loaded over OpenOCD
- Connected to OpenOCD GDB server successfully
- Confirmed UART output via debug probe: "Hello World! Count: N"

### How to run and debug
1. Build and upload:
   - `make` or `make upload`
2. Debug with GDB (two terminals):
   - Terminal 1: `make debug-server`
   - Terminal 2: `make gdb` (loads and breaks at `main`)
3. Listen to UART output:
   - `make uart-monitor` (115200 8N1)

### Session 3
- Added TinyUSB MIDI device support (USB MIDI descriptors + config)
- Kept Hello World UART output in main loop
- Verified MIDI RX via `amidi` and UART prints (Note On/Off)

### Session 4
- Added Makefile MIDI test helpers (`midi-list`, `midi-note-on`, `midi-note-off`, `midi-test`)
- Hardened MIDI RX loop (bounded per-iteration drain)
