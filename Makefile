# MIDI to Solenoid Controller - Makefile
# For RP2040 with Pico Debug Probe

# Project settings
PROJECT_NAME := miditosolenoid
BUILD_DIR := build
DEPS_DIR := deps

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

# Default target
.PHONY: all
all: build

# Configure CMake build
.PHONY: configure
configure:
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

# Upload firmware via OpenOCD
.PHONY: upload
upload: build
	@echo "Uploading $(PROJECT_NAME).elf via debug probe..."
	@$(OPENOCD) $(OPENOCD_ARGS) \
		-c "adapter speed 5000" \
		-c "program $(ELF_FILE) verify reset exit"

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
	@echo "  make upload       - Upload firmware via debug probe"
	@echo "  make debug-server - Start OpenOCD debug server (run in terminal 1)"
	@echo "  make gdb          - Connect GDB and load firmware (run in terminal 2)"
	@echo "  make gdb-attach   - Connect GDB without loading"
	@echo "  make reset        - Reset target"
	@echo "  make halt         - Halt target"
	@echo "  make uart-monitor - Monitor UART output from debug probe"
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
