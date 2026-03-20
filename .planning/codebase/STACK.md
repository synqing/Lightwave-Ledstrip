---
abstract: "Complete technology stack across the LightwaveOS monorepo: 7 subsystems (firmware-v3, iOS, Tab5, web tools) with languages, frameworks, build systems, dependencies, and runtime configurations. Updated 2026-03-21."
---

# Technology Stack

**Analysis Date:** 2026-03-21

## Languages

**Primary:**
- **C++ (17)** — ESP32-S3 firmware core (`firmware-v3/`) — actor model, CQRS, 100+ effects, RTOS tasks
- **Swift (6)** — iOS companion app (`lightwave-ios-v2/`) — iOS 17+ with strict concurrency, @Observable, SwiftUI
- **C** — Embedded utilities, ESP-IDF drivers, FreeRTOS API calls

**Secondary:**
- **Python (3.12+)** — Test harness, audio benchmark tool, LED capture/analysis (`tools/led_capture/`, `firmware-v3/tools/benchmark/`)
- **JavaScript (ES6+)** — k1-composer local proxy, Node.js WebSocket/serial bridge
- **HTML/CSS/JavaScript** — k1-composer web UI (static files, no build step)

## Runtime

**ESP32 Firmware:**
- **Framework:** Arduino (espressif32 v6.9.0 — IDF 4.4.7)
- **Compiler:** xtensa-esp32s3-elf (GCC 11.x via PlatformIO)
- **Optimization:** `-O3 -ffast-math -funroll-loops`
- **Memory:** PSRAM enabled (OPI), littlefs filesystem, custom partition layout (`partitions_custom.csv`)
- **Boot:** OTA-capable with rollback verification (`OtaBootVerifier`)

**iOS:**
- **Runtime:** Objective-C/Swift runtime on iOS 17+
- **SDK:** Xcode 15.0, Swift 6.0 (strict concurrency)
- **Target Device:** iPhone (iOS 17+)

**Tab5 Encoder:**
- **Framework:** Arduino (espressif32 via pioarduino fork #54.03.21)
- **Platform:** ESP32-P4 (RISC-V, with C906 extension support)
- **RAM:** PSRAM enabled

**Web/Proxy:**
- **Runtime:** Node.js 18+ (k1-wired-proxy)
- **Browser:** Modern browser for k1-composer (no specific version requirement)

## Package Managers

**Firmware (ESP32-S3 & Tab5):**
- **PlatformIO** v6+ — dependency resolution, library management, build orchestration
- **Library Registry:** PlatformIO package index
- **Lockfile:** Not used; versions pinned in `platformio.ini`

**iOS:**
- **CocoaPods:** Not used (XcodeGen project generation)
- **Swift Package Manager:** RiveRuntime only (6.12.2+)
- **Lockfile:** Not applicable

**Python Tools:**
- **pip/setuptools** (v75.0+)
- **Lockfile:** `requirements.txt` (led_capture) — minimal, poetry not used

**Node.js (k1-composer proxy):**
- **npm** v9+
- **Lockfile:** `package-lock.json` (not tracked, node_modules gitignored)

## Frameworks & Platforms

### Firmware (firmware-v3)

**Core:**
- **FreeRTOS** (esp-idf 4.4.7) — dual-core task scheduling (Core 0: network/audio, Core 1: rendering), actor model via xQueue
- **Arduino Core** (espressif32 v6.9.0) — USB CDC serial, millis(), delay(), interrupt handlers

**LED Control:**
- **FastLED 3.10.0** — WS2812 RGB control via custom RMT driver (not ESP-IDF rmt_write_items)
  - Custom RMT driver: bypasses spinlock corruption, uses register access + ISR callbacks directly
  - Channel config: `FASTLED_RMT_MAX_CHANNELS=4` (parallel dual-strip output via indices 0 and 2)
  - Memory: `FASTLED_RMT_MEM_BLOCKS=2`, safety semaphore timeout 100ms

**Audio Processing:**
- **ESV11** — 64-bin Goertzel analyzer (ACTIVE production backend)
  - Features: octave bands (0-7), chroma (12-tone chromagram), beat tracking, tempo grid, silence gate (hysteresis)
  - Files: `src/audio/backends/esv11/` — PRODUCTION
- **PipelineCore** — FFT-based analyzer (BROKEN — beat tracking non-functional)
  - Files: `src/audio/backends/pipelinecore/` — DO NOT USE
- **I2S Microphone:** SPH0645 16-bit PCM @ 32 kHz, dual-core data flow (capture→processor→synthesis)

**Networking:**
- **ESPAsyncWebServer 3.9.3** — REST API (47 endpoints), WebSocket real-time control
  - Logging suppressed: `-D ASYNCWEBSERVER_LOG_CUSTOM`
  - Per-client queue: `WS_MAX_QUEUED_MESSAGES=32` (32 frames × 500B × 8 clients = 128KB worst-case)
- **AsyncTCP 3.4.9** — underlying TCP transport, bound to Core 0 (`CONFIG_ASYNC_TCP_RUNNING_CORE=0`)
- **esp_websocket_client** — native ESP-IDF library (no external dep), used by Sync subsystem

**Serialization:**
- **ArduinoJson 7.0.4** — JSON encode/decode for REST/WS payloads, command parsing

**CQRS & State:**
- **Custom event bus** — `ControlBus` (read-only snapshot for effects, write-gated by AudioActor)
- **Command dispatch** — CQRS pattern, state machine for zone control, effect switching
- **Persistence:** NVS (Non-Volatile Storage) via ESP-IDF, structured via `NVSManager`

**Effects & Visualization:**
- **Custom IEffect interface** — 100+ pluggable effects inherit `EffectBase`
- **Zone composition** — `ZoneComposer` with blend modes, per-zone audio reactivity
- **Transition engine** — 12 transition types with smooth interpolation
- **Plugin system** — `PluginManagerActor` for runtime effect loading (binary contracts via codecs)

### iOS (lightwave-ios-v2)

**UI Framework:**
- **SwiftUI** — all views, pure Apple framework (iOS 17+)
- **Canvas** — LED strip preview (320-LED horizontal strip rendering)
- **Rive Runtime 6.12.2** — animated UI assets only (no gameplay animation)

**Concurrency:**
- **Swift Structured Concurrency** — async/await, Tasks, Actors
- **@MainActor** — all ViewModels (@Observable classes)
- **actor** — all network services (RESTClient, WebSocketService, UDPStreamReceiver) for thread safety

**Networking:**
- **URLSession** — REST API calls (`/api/v1/*` endpoints)
- **URLSessionWebSocketTask** — real-time WebSocket (`ws://device/ws`)
- **Network.framework** — device discovery (mDNS/Bonjour via `NWBrowser`)
- **DatagramSocket** — UDP binary frame streaming (optional fallback, port 41234)

**Data & Models:**
- **Codable** — all network models (DeviceInfo, EffectMetadata, PaletteMetadata, etc.)
- **UserDefaults** — persistent app preferences
- **FileManager** — local asset caching (optional)

**Development:**
- **XcodeGen** — Xcode project generation from YAML (project.yml)
- **Xcode 15.0**, Swift 6.0 strict concurrency enabled
- **Build system:** xcodebuild (no Fastlane or other automation)

### Tab5 Encoder (tab5-encoder)

**Display:**
- **LVGL 9.3.0** — lightweight GUI library (touch UI)
- **M5Unified** — M5Stack Tab5 hardware abstraction
- **M5GFX** — graphics layer for LVGL
- **M5ROTATE8** — display rotation utilities

**Connectivity:**
- **WebSockets 2.4.0** — real-time control channel to K1
- **ArduinoJson 7.0.0** — same JSON codec as firmware-v3

**Platform:**
- **Platform:** pioarduino fork (esp32p4 RISC-V support)
- **Build:** `pio run -e tab5` (ESP32-P4 variant with LVGL UI)

### Python Tools

**Audio/LED Analysis:**
- **numpy 1.26.0** — numerical arrays, signal processing
- **scipy 1.14.0** — DSP utilities (filters, correlation)
- **pandas 2.2.0** — time-series data, analysis
- **plotly 5.24.0** — interactive dashboards, real-time data viz

**Serial/Network:**
- **pyserial** — serial port communication (USB CDC)
- **websockets 12.0** — WebSocket client (async)

**Media:**
- **Pillow** — image manipulation, frame capture
- **imageio[ffmpeg]** — video encoding (captured LED frames → MP4)

**Development:**
- **pytest 8.3.0**, **pytest-asyncio 0.24.0**, **pytest-cov 6.0.0** — testing
- **mypy 1.13.0** — static type checking (strict mode)
- **ruff 0.7.0** — linting & formatting

### Node.js Proxy (k1-composer)

**Serial/Networking:**
- **serialport 12.0.0** — USB serial communication
- **@serialport/parser-readline** — frame parsing
- **ws 8.18.0** — WebSocket server (bridges serial ↔ local browser)

**Runtime:**
- **Node.js 18+**
- **ES modules** (native import/export, no transpilation)

## Configuration Files

**Firmware:**
- `firmware-v3/platformio.ini` — build environments (esp32dev_audio_esv11_k1v2_32khz, trace, native_test)
- `firmware-v3/src/config/chip_esp32s3.h` — hardware pin definitions (LED GPIO, I2S pins)
- `firmware-v3/src/audio/backends/esv11/vendor/microphone.h` — I2S configuration
- `firmware-v3/src/config/features.h` — compile-time feature flags (FEATURE_AUDIO_SYNC, FEATURE_WEB_SERVER, etc.)
- `partitions_custom.csv` — ESP32 flash partitioning (app, OTA, NVS, SPIFFS/littlefs)
- `wifi_credentials.ini` — gitignored; copied from template, contains SSID/password

**iOS:**
- `lightwave-ios-v2/project.yml` — XcodeGen project definition (Swift 6, iOS 17, team ID)
- `lightwave-ios-v2/LightwaveOS/Resources/Info.plist` — app metadata (Bonjour services, NSLocalNetworkUsageDescription)

**Tab5:**
- `tab5-encoder/platformio.ini` — ESP32-P4 environment, LVGL configuration
- `tab5-encoder/src/ui/lv_conf.h` — LVGL compile-time settings

**Python Tools:**
- `tools/led_capture/requirements.txt` — minimal pinned versions
- `firmware-v3/tools/benchmark/pyproject.toml` — full PEP 517 project metadata (numpy, scipy, websockets, dash)

**k1-composer:**
- `k1-composer/proxy/package.json` — Node.js proxy dependencies

## Build & Compilation

**Firmware-v3 (ESP32-S3):**
```bash
cd firmware-v3

# Production build (K1v2 with ESV11 audio)
pio run -e esp32dev_audio_esv11_k1v2_32khz

# Build and upload to device
pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload

# Unit tests (host-only, no hardware)
pio run -e native_test

# Serial monitor
pio device monitor -b 115200
```

**Important:**
- DO NOT use `esp32dev_audio_pipelinecore` — beat tracking broken, unreliable audio data
- Always use `_32khz` suffix (never bare `esp32dev_audio_esv11`)
- For K1v2 hardware: `esp32dev_audio_esv11_k1v2_32khz`
- For V1 hardware: `esp32dev_audio_esv11_32khz`

**iOS:**
```bash
cd lightwave-ios-v2

# Generate Xcode project (required after adding files)
xcodegen generate

# Build for iOS Simulator
xcodebuild build -scheme LightwaveOS -destination 'platform=iOS Simulator,name=iPhone 16'

# Run tests
xcodebuild test -scheme LightwaveOS -destination 'platform=iOS Simulator,name=iPhone 16'

# Open in Xcode
open LightwaveOS.xcodeproj
```

**Tab5:**
```bash
cd tab5-encoder

# Build for ESP32-P4 + LVGL
pio run -e tab5

# Upload
pio run -e tab5 -t upload

# Debug build
pio run -e tab5_debug -t upload
```

**Python Tools:**
```bash
cd tools/led_capture
pip install -r requirements.txt
python led_capture.py stream b 30  # Capture at 30 FPS

cd firmware-v3/tools/benchmark
uv run lwos-bench collect --ws ws://192.168.4.1/ws
```

## Hardware Requirements

**Development:**
- ESP32-S3 DevKit (N16R8 — 16MB flash, 8MB PSRAM, USB-C)
- SPH0645 I2S microphone (or simulator for testing)
- WS2812 LED strips (dual 160-LED strips for K1)
- K1v2 hardware: GPIO 6/7 for LEDs, GPIO 11/13/14 for I2S, GPIO 21 for display RST (optional)
- Tab5 Encoder: M5Stack Tab5 with ESP32-P4
- Mac with Xcode 15+, Python 3.12+, Node.js 18+

**Production Deployment:**
- K1 device: ESP32-S3 + dual WS2812 strips (AP-only WiFi at 192.168.4.1)
- iOS device: iPhone XS+ with iOS 17+
- Local network (no internet required)

## Platform-Specific Notes

**ESP32-S3 Audio & Timing:**
- 120 FPS target: effects must render in < 2.0 ms
- No heap allocation in `render()` — use static buffers only
- Centre origin: LEDs 79/80 (physical centre of 160-strip), effects radiate outward
- Audio: 64-bin Goertzel features (octave bands 0-7, chroma 0-11), backend-agnostic via ControlBus

**iOS:**
- Strict concurrency enabled — all async work in Tasks, UI on @MainActor
- WebSocket for commands (reliable), UDP optional for LED stream (binary data)
- Debounce: 150ms minimum before sending slider changes (prevents flooding)
- LED preview: Canvas-based, 320 pixels, horizontal or concentric ring mode

**WiFi Architecture (CRITICAL):**
- K1 is **AP-ONLY** — never add STA mode. Router connection fails at 802.11 driver level (AUTH_EXPIRE, AUTH_FAIL).
- K1 broadcasts open AP "LightwaveOS-AP" at 192.168.4.1
- Tab5 and iOS connect as STA clients to K1's AP
- No internet connectivity required — local network only

---

*Stack analysis: 2026-03-21*
