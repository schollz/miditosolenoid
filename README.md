# MIDI to Solenoid Controller

RP2040-based MIDI device for controlling solenoids. Uses a Raspberry Pi Pico Debug Probe (CMSIS-DAP) for SWD and UART.

## Prerequisites
- Pico Debug Probe connected to the target via SWD
- UART wired to debug probe: GP0=TX, GP1=RX

## Build
```bash
make
```

## Upload (OpenOCD)
```bash
make upload
```

## Debugger
Run OpenOCD in one terminal:
```bash
make debug-server
```

Connect GDB in another terminal (loads firmware, breaks at main):
```bash
make gdb
```

If you only want to attach without loading:
```bash
make gdb-attach
```

Helpful GDB commands:
```
monitor reset run
monitor reset halt
bt
info registers
```

## UART Debug Output (Hello World)
The firmware prints over UART at 115200 8N1.

Quick monitor:
```bash
make uart-monitor
```

Find the debug probe UART device:
```bash
ls /dev/serial/by-id
```

Example read:
```bash
UART_DEV=/dev/serial/by-id/usb-Raspberry_Pi_Debug_Probe__CMSIS-DAP__E6632891E3889E30-if01
stty -F "$UART_DEV" 115200 cs8 -cstopb -parenb -echo -icanon -isig -iexten
cat "$UART_DEV"
```

You should see lines like:
```
Hello World! Count: 0
```
