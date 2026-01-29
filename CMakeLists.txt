# MIDI to Solenoid Controller
# CMakeLists.txt for Pico SDK 2.0.0

cmake_minimum_required(VERSION 3.13)

# Set PICO_SDK_PATH to local dependency before including pico_sdk_import
set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/deps/pico-sdk)

# Include the Pico SDK import script
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Project definition
project(miditosolenoid C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Add executable
add_executable(miditosolenoid
    src/main.cpp
)

# Enable UART output for printf (default pins GP0=TX, GP1=RX)
pico_enable_stdio_uart(miditosolenoid 1)
pico_enable_stdio_usb(miditosolenoid 0)

# Link required libraries
target_link_libraries(miditosolenoid
    pico_stdlib
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(miditosolenoid)

# Print memory usage after build
# target_link_options(miditosolenoid PRIVATE -Wl,--print-memory-usage)
