# MIDI to Solenoid Controller - Makefile
# For RP2040 with Pico Debug Probe

# Project settings
PROJECT_NAME := miditosolenoid
BUILD_DIR := build
DEPS_DIR := deps
PICO_SDK_DIR := $(DEPS_DIR)/pico-sdk
PICO_SDK_IMPORT := $(PICO_SDK_DIR)/external/pico_sdk_import.cmake

# Tools
CMAKE := cmake
OPENOCD := openocd
GDB := gdb-multiarch

# OpenOCD configuration for Pico Debug Probe (CMSIS-DAP)
OPENOCD_INTERFACE := interface/cmsis-dap.cfg
OPENOCD_TARGET := target/rp2040.cfg
OPENOCD_ARGS := -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET)

# GDB settings
GDB_PORT := 3333
ELF_FILE := $(BUILD_DIR)/$(PROJECT_NAME).elf
UF2_FILE := $(BUILD_DIR)/$(PROJECT_NAME).uf2
AMIDI := amidi

# USB bootloader upload settings
RPI_RP2_MOUNT := /media/$(USER)/RPI-RP2

# Default target
.PHONY: all
all: build

# Configure CMake build
.PHONY: configure
configure: deps
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) ..

# Build the project
.PHONY: build
build: configure
	@$(CMAKE) --build $(BUILD_DIR) --parallel

# Clean build directory only
.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)
	@echo "Build directory cleaned"

# Clean everything including dependencies
.PHONY: cleanall
cleanall: clean
	@rm -rf $(DEPS_DIR)
	@echo "Dependencies cleaned"

# Install Pico SDK locally if missing
.PHONY: deps
deps:
	@if [ ! -f "$(PICO_SDK_IMPORT)" ]; then \
		echo "Pico SDK not found, installing into $(PICO_SDK_DIR) ..."; \
		mkdir -p "$(DEPS_DIR)"; \
		git clone --depth 1 --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git "$(PICO_SDK_DIR)"; \
		git -C "$(PICO_SDK_DIR)" submodule update --init --recursive; \
	else \
		echo "Pico SDK found at $(PICO_SDK_DIR)"; \
	fi

# Upload firmware via OpenOCD, fallback to USB bootloader if debug probe unavailable
.PHONY: upload
upload: build
	@echo "Uploading $(PROJECT_NAME).elf via debug probe..."
	@$(OPENOCD) $(OPENOCD_ARGS) \
		-c "adapter speed 5000" \
		-c "program $(ELF_FILE) verify reset exit" 2>/dev/null || { \
		echo "Debug probe not available, falling back to USB bootloader..."; \
		$(MAKE) upload-usb; \
	}

# Upload firmware via USB bootloader (1200 baud touch -> copy UF2)
.PHONY: upload-usb
upload-usb: build
	@echo "Triggering USB bootloader mode..."
	@SERIAL_DEV=$$(ls /dev/ttyACM* 2>/dev/null | head -n 1); \
	if [ -n "$$SERIAL_DEV" ]; then \
		echo "Opening $$SERIAL_DEV at 1200 baud to enter bootloader..."; \
		stty -F "$$SERIAL_DEV" 1200; \
		sleep 0.1; \
	else \
		echo "No /dev/ttyACM* found. Please manually enter bootloader mode (hold BOOTSEL + reset)."; \
	fi
	@echo "Waiting for RPI-RP2 to mount..."
	@for i in $$(seq 1 30); do \
		if [ -d "$(RPI_RP2_MOUNT)" ]; then \
			break; \
		fi; \
		sleep 0.5; \
	done
	@if [ ! -d "$(RPI_RP2_MOUNT)" ]; then \
		echo "Error: $(RPI_RP2_MOUNT) did not appear after 15 seconds"; \
		exit 1; \
	fi
	@echo "Copying $(UF2_FILE) to $(RPI_RP2_MOUNT)..."
	@cp "$(UF2_FILE)" "$(RPI_RP2_MOUNT)/"
	@sync
	@echo "Upload complete!"

# Start OpenOCD debug server (run in separate terminal)
.PHONY: debug-server
debug-server:
	@echo "Starting OpenOCD debug server on port $(GDB_PORT)..."
	@echo "Press Ctrl+C to stop"
	@$(OPENOCD) $(OPENOCD_ARGS) \
		-c "adapter speed 5000"

# Connect GDB to debug server (run after debug-server)
.PHONY: gdb
gdb: build
	@echo "Connecting GDB to localhost:$(GDB_PORT)..."
	@$(GDB) $(ELF_FILE) \
		-ex "target extended-remote localhost:$(GDB_PORT)" \
		-ex "monitor reset halt" \
		-ex "load" \
		-ex "break main" \
		-ex "continue"

# Quick GDB connect without loading (for attaching to running target)
.PHONY: gdb-attach
gdb-attach:
	@$(GDB) $(ELF_FILE) \
		-ex "target extended-remote localhost:$(GDB_PORT)"

# Reset target via OpenOCD
.PHONY: reset
reset:
	@$(OPENOCD) $(OPENOCD_ARGS) \
		-c "adapter speed 5000" \
		-c "init" \
		-c "reset run" \
		-c "exit"

# Halt target
.PHONY: halt
halt:
	@$(OPENOCD) $(OPENOCD_ARGS) \
		-c "adapter speed 5000" \
		-c "init" \
		-c "halt" \
		-c "exit"

# Show help
.PHONY: help
help:
	@echo "MIDI to Solenoid Controller - Build System"
	@echo ""
	@echo "Build targets:"
	@echo "  make              - Build the project"
	@echo "  make build        - Build the project"
	@echo "  make configure    - Configure CMake only"
	@echo "  make clean        - Clean build directory"
	@echo "  make cleanall     - Clean build and dependencies"
	@echo ""
	@echo "Upload/Debug targets:"
	@echo "  make upload       - Upload firmware (debug probe, fallback to USB)"
	@echo "  make upload-usb   - Upload firmware via USB bootloader (UF2)"
	@echo "  make debug-server - Start OpenOCD debug server (run in terminal 1)"
	@echo "  make gdb          - Connect GDB and load firmware (run in terminal 2)"
	@echo "  make gdb-attach   - Connect GDB without loading"
	@echo "  make reset        - Reset target"
	@echo "  make halt         - Halt target"
	@echo "  make uart-monitor - Monitor UART output from debug probe"
	@echo "  make midi-list    - List ALSA MIDI devices"
	@echo "  make midi-note-on - Send MIDI Note On (NOTE=60 VEL=64)"
	@echo "  make midi-note-off- Send MIDI Note Off (NOTE=60)"
	@echo "  make midi-test    - Send a quick Note On/Off test sequence"
	@echo ""
	@echo "Debug workflow:"
	@echo "  1. Terminal 1: make debug-server"
	@echo "  2. Terminal 2: make gdb"

# Monitor UART output via debug probe (GP0/GP1 at 115200 8N1)
.PHONY: uart-monitor
uart-monitor:
	@UART_DEV=$$(ls /dev/serial/by-id/*CMSIS-DAP* 2>/dev/null | head -n 1); \
	if [ -z "$$UART_DEV" ]; then \
		echo "No CMSIS-DAP UART device found in /dev/serial/by-id"; \
		exit 1; \
	fi; \
	echo "Using UART device: $$UART_DEV"; \
	stty -F "$$UART_DEV" 115200 cs8 -cstopb -parenb -echo -icanon -isig -iexten; \
	cat "$$UART_DEV"

# MIDI helpers (requires amidi + permissions to access ALSA MIDI)
.PHONY: midi-list midi-note-on midi-note-off midi-test
midi-list:
	@$(AMIDI) -l || true

midi-note-on:
	@DEV=$$($(AMIDI) -l 2>/dev/null | awk '/MIDITOUSB/ {print $$2; exit}'); \
	if [ -z "$$DEV" ]; then \
		echo "MIDITOUSB device not found (run: make midi-list)"; \
		exit 1; \
	fi; \
	NOTE=$${NOTE:-60}; VEL=$${VEL:-64}; \
	printf "Using $$DEV: Note On %s vel %s\n" "$$NOTE" "$$VEL"; \
	$(AMIDI) -p $$DEV -S "$$(printf '90 %02X %02X' $$NOTE $$VEL)"

midi-note-off:
	@DEV=$$($(AMIDI) -l 2>/dev/null | awk '/MIDITOUSB/ {print $$2; exit}'); \
	if [ -z "$$DEV" ]; then \
		echo "MIDITOUSB device not found (run: make midi-list)"; \
		exit 1; \
	fi; \
	NOTE=$${NOTE:-60}; \
	printf "Using $$DEV: Note Off %s\n" "$$NOTE"; \
	$(AMIDI) -p $$DEV -S "$$(printf '80 %02X 00' $$NOTE)"

midi-test:
	@DEV=$$($(AMIDI) -l 2>/dev/null | awk '/MIDITOUSB/ {print $$2; exit}'); \
	if [ -z "$$DEV" ]; then \
		echo "MIDITOUSB device not found (run: make midi-list)"; \
		exit 1; \
	fi; \
	printf "Using $$DEV: Note On/Off test\n"; \
	$(AMIDI) -p $$DEV -S "90 3C 40"; \
	sleep 0.1; \
	$(AMIDI) -p $$DEV -S "80 3C 00"; \
	sleep 0.1; \
	$(AMIDI) -p $$DEV -S "90 40 7F"; \
	sleep 0.1; \
	$(AMIDI) -p $$DEV -S "90 40 00"
