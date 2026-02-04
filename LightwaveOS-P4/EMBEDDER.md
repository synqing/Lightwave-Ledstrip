# EMBEDDER PROJECT CONTEXT

<OVERVIEW>
Name = LightwaveOS-P4
Target MCU = ESP32-P4
Board = Waveshare ESP32-P4-WIFI6
Toolchain = ESP-IDF v5.5.2 with RISC-V GCC (riscv32-esp-elf-gcc esp-14.2.0)
Debug Interface = serial
RTOS / SDK = FreeRTOS (via ESP-IDF v5.5.2)
Project Summary = Music visualizer controlling 2 WS2812 LED strips (160 LEDs each, 320 total) with real-time audio DSP
</OVERVIEW>

<COMMANDS>
# --- Build / Compile --------------------------------------------------------
# Note: Build scripts enforce ESP-IDF v5.5.2 and auto-configure Python environment
build_command = ./build_with_idf55.sh

# --- Flash ------------------------------------------------------------------
# Interactive script that guides through bootloader mode and flashing
flash_command = ./flash_and_monitor.sh

# Alternative: Direct ESP-IDF commands (requires IDF environment setup)
# idf.py -p /dev/tty.usbmodem5AAF2781791 flash
# idf.py -p /dev/tty.usbmodem5AAF2781791 monitor

# --- Debug ------------------------------------------------------------------
# Serial-based debugging only (ESP32-P4 uses UART for debug output)
gdb_server_command = N/A
gdb_server_host = N/A
gdb_server_port = N/A
gdb_client_command = N/A
target_connection = N/A

# --- Serial Monitor ----------------------------------------------------------
serial_port = /dev/tty.usbmodem5AAF2781791
serial_baudrate = 115200
serial_monitor_command = python -c "import serial; import sys; ser = serial.Serial('{port}', {baud}, timeout=1); [sys.stdout.write(ser.read(ser.in_waiting).decode('utf-8', errors='replace')) or sys.stdout.flush() for _ in iter(int, 1)]"
serial_monitor_interactive = false
serial_encoding = utf-8
serial_startup_commands = []

# Alternative monitor via ESP-IDF
# idf.py -p /dev/tty.usbmodem5AAF2781791 monitor
</COMMANDS>

## Project Overview

LightwaveOS-P4 is a music visualization system built on ESP32-P4 that drives dual WS2812 LED strips (320 LEDs total) in real-time. The project uses ESP-IDF v5.5.2 with FreeRTOS and leverages ESP-DSP for audio signal processing and FFT operations.

**Architecture:**
- **Target Hardware**: Waveshare ESP32-P4-WIFI6 development board
- **LED Control**: RMT peripheral for precise WS2812 timing (GPIO 20/21)
- **Audio Processing**: ESP-DSP library for FFT and real-time audio analysis
- **Build System**: ESP-IDF v5.5.2 with CMake
- **Components**: FastLED port, Arduino compatibility layer, esp-dsp, led_strip

**Key Features:**
- Dual LED strip control (160 LEDs per strip)
- Real-time audio FFT processing
- FreeRTOS task scheduling
- ESP-IDF v5.5.2 managed components (esp-dsp, led_strip)

**Project Structure:**
```
LightwaveOS-P4/              # Build scripts directory (this location)
├── build_with_idf55.sh      # Main build script (enforces IDF v5.5.2)
├── flash_and_monitor.sh     # Interactive flash and serial monitor
├── verify_build.sh          # Verify build environment
└── EMBEDDER.md              # This file

firmware/v2/esp-idf-p4/      # Source code location
├── main/                    # Application entry point
├── components/              # Custom components (FastLED, arduino-compat)
└── managed_components/      # ESP-IDF managed deps (esp-dsp, led_strip)
```

## Bash Commands

**Build:**
```bash
./build_with_idf55.sh
# Automatically: checks IDF version, sets up Python env, cleans build, compiles
```

**Flash and Monitor:**
```bash
./flash_and_monitor.sh
# Interactive: guides through bootloader mode, flashes, monitors serial
```

**Verify Environment:**
```bash
./verify_build.sh
# Checks IDF_PATH, version, dependencies
```

**Direct ESP-IDF Commands** (requires sourcing export.sh):
```bash
cd /Users/spectrasynq/Workspace_Management/Software/Lightwave-Ledstrip/firmware/v2/esp-idf-p4
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/tty.usbmodem5AAF2781791 flash monitor
```

**Manual Bootloader Entry** (for flashing):
1. Press and HOLD the BOOT button
2. Press and RELEASE the RESET button (while holding BOOT)
3. RELEASE the BOOT button

## Code Style

**Language:** C (C11 standard with GNU extensions)

**Formatting:**
- Indentation: 4 spaces (no tabs)
- Line length: 120 characters max
- Brace style: K&R (opening brace on same line for functions)
- Naming conventions:
  - Functions: `snake_case`
  - Variables: `snake_case`
  - Macros/Constants: `UPPER_CASE`
  - Type definitions: `snake_case_t`
  - GPIO defines: `LED_STRIP1_GPIO` style

**ESP-IDF Conventions:**
- Use `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` for logging
- Always check return values with `ESP_ERROR_CHECK()`
- Use FreeRTOS types: `TaskHandle_t`, `QueueHandle_t`, etc.
- Include ESP-IDF headers before standard library: `#include "freertos/FreeRTOS.h"` before `#include <stdio.h>`

**Comments:**
- Single-line: `// Comment`
- Multi-line: `/* Comment */`
- Document complex algorithms and hardware-specific configurations
- Tag tasks: `TODO:`, `FIXME:`, `NOTE:`

**Header Guards:**
```c
#ifndef COMPONENT_NAME_H
#define COMPONENT_NAME_H
// ...
#endif // COMPONENT_NAME_H
```

**Component Structure:**
```
components/component_name/
├── CMakeLists.txt
├── include/component_name.h
└── component_name.c
```

**Error Handling:**
- Use ESP-IDF error codes (`esp_err_t`)
- Check and propagate errors appropriately
- Log errors with context: `ESP_LOGE(TAG, "Failed to init LED: %s", esp_err_to_name(ret))`

**No Cursor/Copilot Rules Found:**
This project does not have `.cursorrules` or `.github/copilot-instructions.md` files defined.
