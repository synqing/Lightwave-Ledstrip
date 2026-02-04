ESP32-P4 Firmware - Complete Code Inventory
Project Overview
LightwaveOS-P4 is a music visualization firmware for ESP32-P4 that controls dual WS2812 LED strips (320 LEDs total) with real-time audio DSP processing. It's built on ESP-IDF v5.5.2 with a custom actor-based architecture ported from the main LightwaveOS v2 codebase.
---
1. Main Application Code
esp-idf-p4/main/main.cpp (520 lines)
Primary P4 entry point - Full-featured music visualizer application
- Architecture: Actor-based system with RendererActor for LED control
- Features:
  - Serial command interface (effect selection, brightness, speed, palette control)
  - Zone system (split LED strips into independent zones)
  - Effect registry with 80+ effects (reactive/ambient)
  - NVS persistence for settings
  - Real-time audio processing integration
- Key Functions:
  - app_main(): Initializes actor system, registers effects, starts main loop
  - handleSerialCommands(): Interactive serial menu for runtime control
  - printSerialCommands(): Help text for available commands
- System Integration:
  - Uses shared codebase from ../../src/ (audio, effects, core)
  - Excludes network/OTA features (P4-specific build config)
esp-idf-p4/main/main.c (67 lines)
Minimal LED test/bringup code (legacy/alternate entry point)
- Simple WS2812 heartbeat test
- Toggles strip1 (red) and strip2 (green) every 500ms
- Uses ESP-IDF led_strip component directly (not FastLED)
- Note: Currently unused; main.cpp is the active entry point
---
2. Hardware Abstraction Layer (HAL)
src/hal/esp32p4/LedDriver_P4.cpp/h (201 + 68 lines)
P4-specific LED driver implementation
- Hardware Configuration:
  - GPIO 20: LED Strip 1 (160 LEDs max)
  - GPIO 21: LED Strip 2 (160 LEDs max)
  - Uses FastLED with RMT5 backend for precise WS2812 timing
- Key Features:
  - Single or dual-strip initialization
  - Per-strip buffer management (320 LED capacity)
  - Brightness control, color correction, power limiting
  - Performance statistics (show time, FPS tracking)
- FastLED Configuration:
  - WS2812 chipset, GRB color order
  - 10 MHz RMT clock resolution
  - Dithering enabled, unlimited refresh rate
src/hal/HalFactory.h
Factory pattern for platform-specific HAL selection (creates LedDriver_P4 on ESP32-P4)
---
3. Custom Components (ESP-IDF)
components/arduino-compat/
Minimal Arduino compatibility layer for ESP-IDF
- Files:
  - Arduino.cpp (59 bytes) - Stub implementation
  - include/Arduino.h - Core Arduino types/macros
  - include/Preferences.h - NVS wrapper
  - include/pgmspace.h - PROGMEM compatibility
- Purpose: Allows Arduino-style code (Serial, delay, millis) in ESP-IDF
components/FastLED/
Vendored FastLED library with ESP32-P4 RMT5 support
- CMake Configuration:
  - Includes core FastLED sources + ESP32 platform code
  - RMT5 backend for P4 (src/platforms/esp/32/rmt_5/*.cpp)
  - Third-party ESP-IDF led_strip helpers
- Defines:
  - ESP32=1, FASTLED_ESP32=1, CONFIG_IDF_TARGET_ESP32P4=1
  - F_CPU=360000000L (P4 clock speed)
  - FASTLED_RMT_BUILTIN_DRIVER=1 (use RMT5)
---
4. Build Configuration
esp-idf-p4/main/CMakeLists.txt (45 lines)
P4-specific build configuration
- Source Code:
  - Recursively includes all .cpp/.c files from ../../src/
  - Excludes network, sync, codec, plugins modules (P4 is serial-only)
  - Uses main.cpp as entry point (not Arduino main.cpp)
- Dependencies: arduino-compat, FastLED, driver, esp_timer, nvs_flash, es8311
- Feature Flags:
    CHIP_ESP32_P4=1
  FEATURE_WEB_SERVER=0
  FEATURE_MULTI_DEVICE=0
  FEATURE_OTA_UPDATE=0
  FEATURE_HAL_ABSTRACTION=1
  FASTLED_RMT_BUILTIN_DRIVER=1
  
esp-idf-p4/dependencies.lock
Managed component versions:
- espressif/esp-dsp v1.7.0 (FFT, DSP operations)
- espressif/led_strip v3.0.2 (RMT LED control)
- espressif/es8311 v1.0.0 (Audio codec driver)
- IDF v5.5.2 (locked)
esp-idf-p4/CMakeLists.txt (5 lines)
Root project file - standard ESP-IDF project structure
---
5. Source Code Architecture (Shared with S3)
The P4 build reuses most of the LightwaveOS v2 codebase from firmware/v2/src/:
Core Systems (src/core/)
- Actor System: Message-passing concurrency (ActorSystem, RendererActor, AudioActor)
- Persistence: NVS storage for settings (NVSManager, ZoneConfigManager)
- Effects Engine: Plugin-based effect system with 80+ patterns
Audio Processing (src/audio/)
- AudioActor: I2S audio capture, FFT analysis, beat detection
- AudioCapture: Real-time audio sampling from ES8311 codec
- GoertzelAnalyzer: Frequency-domain analysis
- StyleDetector: Music genre/tempo detection
- TempoTracker: BPM detection
Effects (src/effects/)
- 80+ registered effects (reactive + ambient)
- Pattern registry with effect categorization
- Zone composer for multi-zone control
Hardware (src/hardware/)
- Performance monitoring
- Encoder management (rotary encoders for UI)
Utilities (src/utils/)
- Logging macros (LW_LOGI, LW_LOGE)
- Color utilities, math helpers
---
6. Build Scripts (LightwaveOS-P4/)
build_with_idf55.sh (253 lines)
Enforces ESP-IDF v5.5.2 environment
- Auto-detects IDF v5.5.2 installation
- Sets up Python virtual environment (prevents v6.0 contamination)
- Adds toolchain to PATH (riscv32-esp-elf-gcc esp-14.2.0)
- Cleans build artifacts aggressively
- Runs idf.py set-target esp32p4 && idf.py build
flash_and_monitor.sh (89 lines)
Interactive flash + serial monitor
- Guides user through bootloader mode (BOOT+RESET button sequence)
- Flashes binary at 0x20000 (app partition)
- Launches Python serial monitor (115200 baud)
verify_build.sh (50 lines)
Checks IDF version, esp-dsp dependency, build artifacts
---
7. P4-Specific Features
Hardware Differences from ESP32-S3
- No WiFi/BLE: P4 is wired-only (serial control instead of web server)
- Dual RMT channels: Independent control of 2 LED strips
- 360 MHz CPU: Faster than S3 (240 MHz)
- RISC-V architecture: Uses riscv32-esp-elf toolchain (not Xtensa)
Feature Gating (src/config/features.h)
P4-specific overrides:
#if defined(CHIP_ESP32_P4)
    #undef FEATURE_WEB_SERVER
    #define FEATURE_WEB_SERVER 0
    #undef FEATURE_MULTI_DEVICE
    #define FEATURE_MULTI_DEVICE 0
#endif
Serial Control Interface
Since P4 lacks WiFi, all control is via serial commands:
- Effect selection (0-9, a-k)
- Brightness (+/-)
- Speed (/)
- Palette (,/.)
- Zone mode (z, 1-5 presets)
- Save settings (S)
---
8. Key Technical Details
LED Configuration
- Strip 1: GPIO 20, up to 160 WS2812 LEDs
- Strip 2: GPIO 21, up to 160 WS2812 LEDs
- Total capacity: 320 LEDs (2x160)
- Protocol: WS2812B (800 kHz, GRB order)
- Driver: FastLED via RMT5 peripheral
Audio Processing
- Codec: ES8311 (I2S interface)
- DSP Library: ESP-DSP v1.7.0 (Espressif)
- FFT: Real-time frequency analysis
- Features: Beat detection, tempo tracking, style detection
Memory Layout
- Flash: Binary at 0x20000 (standard app partition)
- Heap: FreeRTOS heap for dynamic allocation
- LED Buffers: Static allocation (2x160 CRGB = 1920 bytes)
Build Toolchain
- IDF Version: v5.5.2 (strictly enforced)
- Compiler: riscv32-esp-elf-gcc esp-14.2.0_20251107
- CMake: v3.16+
- Python: idf5.5_py3.x (isolated environment)
---
9. File Tree Summary
LightwaveOS-P4/                      # Build infrastructure
├── build_with_idf55.sh              # Main build script
├── flash_and_monitor.sh             # Flash + monitor script
├── verify_build.sh                  # Environment verification
└── EMBEDDER.md                      # Project documentation
firmware/v2/
├── esp-idf-p4/                      # ESP-IDF P4 project root
│   ├── main/
│   │   ├── main.cpp                 # Primary P4 application (520 lines)
│   │   ├── main.c                   # Legacy LED test (67 lines)
│   │   ├── CMakeLists.txt           # Build config with feature flags
│   │   └── idf_component.yml        # Dependency manifest
│   ├── components/
│   │   ├── arduino-compat/          # Arduino API shim
│   │   │   ├── Arduino.cpp
│   │   │   ├── CMakeLists.txt
│   │   │   └── include/             # Arduino.h, Preferences.h
│   │   └── FastLED/                 # FastLED library (RMT5 support)
│   │       ├── CMakeLists.txt       # P4-specific FastLED config
│   │       └── src/                 # FastLED sources
│   ├── managed_components/          # Auto-downloaded dependencies
│   │   ├── espressif__esp-dsp/      # DSP library (FFT, etc.)
│   │   ├── espressif__led_strip/    # LED strip driver
│   │   └── espressif__es8311/       # Audio codec driver
│   ├── CMakeLists.txt               # Project root CMakeLists
│   ├── dependencies.lock            # Locked dependency versions
│   └── sdkconfig                    # ESP-IDF configuration
└── src/                             # Shared codebase (S3 + P4)
    ├── hal/
    │   └── esp32p4/
    │       ├── LedDriver_P4.cpp     # P4 LED driver (201 lines)
    │       └── LedDriver_P4.h       # Driver interface (68 lines)
    ├── audio/                       # Audio processing subsystem
    ├── core/                        # Actor system, persistence
    ├── effects/                     # 80+ LED effects
    ├── config/                      # Feature flags, chip config
    └── utils/                       # Logging, helpers
---
10. Lines of Code (P4-Specific)
┌─────────────────────┬───────────────────────────────────┬──────────────┐
│ Category            │ Files                             │ Lines        │
├─────────────────────┼───────────────────────────────────┼──────────────┤
│ Main Application    │ main.cpp, main.c                  │ 587          │
├─────────────────────┼───────────────────────────────────┼──────────────┤
│ HAL (P4 LED Driver) │ LedDriver_P4.*                    │ 269          │
├─────────────────────┼───────────────────────────────────┼──────────────┤
│ Arduino Compat      │ arduino-compat/*                  │ ~500         │
├─────────────────────┼───────────────────────────────────┼──────────────┤
│ Build Config        │ CMakeLists.txt, *.yml             │ ~100         │
├─────────────────────┼───────────────────────────────────┼──────────────┤
│ Build Scripts       │ build.sh, flash.sh                │ ~350         │
├─────────────────────┼───────────────────────────────────┼──────────────┤
│ FastLED Integration │ components/FastLED/CMakeLists.txt │ 36           │
├─────────────────────┼───────────────────────────────────┼──────────────┤
│ Total P4-Specific   │                                   │ ~1,842 lines │
└─────────────────────┴───────────────────────────────────┴──────────────┘
Note: The majority of functionality (~50,000+ lines) comes from the shared src/ codebase, which is platform-agnostic and compiled for both ESP32-S3 and ESP32-P4.
---
Summary
The ESP32-P4 port is a lightweight, serial-controlled variant of LightwaveOS v2 that:
1. Reuses 95% of the S3 codebase (audio, effects, core systems)
2. Adds P4-specific HAL (LedDriver_P4 with RMT5 support)
3. Removes network features (no WiFi on P4)
4. Uses ESP-IDF v5.5.2 exclusively (enforced by build scripts)
5. Provides serial UI for all controls (no web interface)
The architecture is clean, modular, and demonstrates good separation of concerns between platform-specific (HAL) and portable (effects/audio) code.