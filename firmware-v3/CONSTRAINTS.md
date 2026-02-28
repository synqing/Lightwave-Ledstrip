# Constraints (Hard Limits)

This file contains extracted constraints from code and documentation. Agents must respect these limits.

---

## Timing Budget

| Operation | Budget | Actual | Source |
|-----------|--------|--------|--------|
| **Frame total (raw)** | 8.33ms (120 FPS target) | ~7.7ms on baseline effects | Measured (2026-02-28) |
| Effect calculation | 2ms max | ~1.0-1.1ms typical | Measured (2026-02-28) |
| Transition processing | 1ms max | ~0.8ms typical | Measured |
| FastLED.show() | Fixed (wire + driver) | ~6.3ms (dual 160 + status 30) | Measured (2026-02-28) |
| LED wire transfer | Fixed | ~4.8ms (2x160 parallel) + status overhead | WS2812 spec + measurement |

**Rule:** Effect code must complete in < 2ms to maintain 120 FPS.

---

## Memory Budget

| Resource | Limit | Why | Source |
|----------|-------|-----|--------|
| **Total SRAM** | 320KB | ESP32-S3 spec | Hardware |
| **Available after boot** | ~280KB | FreeRTOS + framework overhead | Measured |
| **LED buffer** | 960 bytes | 320 LEDs × 3 bytes (RGB) | hardware_config.h |
| **Transition buffer** | 960 bytes | Same as LED buffer | hardware_config.h |
| **Max effects** | 104 | `MAX_EFFECTS` constant | RendererActor.h |
| **Zone buffer** | 480 bytes | 160 LEDs × 3 bytes (per zone) | hardware_config.h |

**Rule:** No dynamic allocation in render loops. Use static buffers.

---

## LED Configuration

| Parameter | Value | Source |
|-----------|-------|--------|
| **Total LEDs** | 320 | hardware_config.h |
| **LEDs per strip** | 160 | hardware_config.h |
| **Center point** | LED 79/80 | hardware_config.h |
| **Segments per strip** | 8 | hardware_config.h |
| **LEDs per segment** | 20 | hardware_config.h |
| **Max zones** | 4 | hardware_config.h |
| **LEDs per zone** | 40 | hardware_config.h |

---

## Power Budget

| Condition | Limit | Source |
|-----------|-------|--------|
| **Max brightness** | 160/255 | Current limiting for 320 LEDs |
| **Default brightness** | 96/255 | Safe operating point |
| **Max current per LED** | 60mA @ full white | WS2812 spec |
| **Total max current** | 19.2A theoretical | 320 × 60mA |
| **Practical max** | ~5A | Brightness limiting applied |

**Rule:** Never set brightness > 160 without external power supply considerations.

---

## Network/API

| Parameter | Value | Source |
|-----------|-------|--------|
| **WebSocket port** | 80 | WebServer.cpp |
| **REST API port** | 80 | WebServer.cpp |
| **mDNS hostname** | lightwaveos.local | network_config.h |
| **Max WebSocket clients** | 4 | ESPAsyncWebServer default |

### API Endpoints
```
POST /api/effect      - Set effect by ID (0-based index)
POST /api/brightness  - Set brightness (0-255)
POST /api/speed       - Set animation speed (1-50)
POST /api/palette     - Select palette by ID
POST /api/zone/count  - Set zone count (1-4)
POST /api/zone/effect - Set zone-specific effect
```

---

## Code Constraints

| Constraint | Enforced By | Notes |
|------------|-------------|-------|
| **C++17 required** | platformio.ini | `-std=gnu++17` |
| **No exceptions** | ESP-IDF config | Embedded constraint |
| **No RTTI** | ESP-IDF config | Reduces binary size |
| **FreeRTOS aware** | Framework | Must not block Core 0 |

### Forbidden Patterns

```cpp
// BAD: Dynamic allocation in render
void effect() {
    CRGB* buffer = new CRGB[160];  // FORBIDDEN
    // ...
    delete[] buffer;
}

// GOOD: Static allocation
void effect() {
    static CRGB buffer[160];  // OK
    // ...
}
```

```cpp
// BAD: Linear iteration (violates CENTER ORIGIN)
for (int i = 0; i < 160; i++) {
    leds[i] = color;  // Left-to-right FORBIDDEN
}

// GOOD: Center-out iteration
for (int i = 0; i < 80; i++) {
    leds[79 - i] = color;  // Left from center
    leds[80 + i] = color;  // Right from center
}
```

---

## Build Constraints

| Constraint | Value | Source |
|------------|-------|--------|
| **Flash usage** | < 80% recommended | 3.34MB total |
| **Current flash** | ~33.2% (2.17MB) | Build `esp32dev_audio_pipelinecore` (2026-02-28) |
| **RAM usage** | < 80% recommended | 320KB total |
| **Current RAM** | ~30.3% (99KB) | Build `esp32dev_audio_pipelinecore` (2026-02-28) |

---

## Feature Flags

These compile-time flags control what's included:

| Flag | Default | Effect |
|------|---------|--------|
| `FEATURE_WEB_SERVER` | 1 (on) | WiFi/WebServer (enabled in all hardware builds) |
| `FEATURE_SERIAL_MENU` | 1 (on) | Serial commands |
| `FEATURE_PERFORMANCE_MONITOR` | 1 (on) | FPS tracking |
| `FEATURE_INTERFERENCE_CALC` | 1 (on) | Wave physics |
| `FEATURE_PHYSICS_SIMULATION` | 1 (on) | Physics effects |

WiFi and WebServer are enabled by default in all hardware build environments via common build flags in platformio.ini.

---

## Discovered Constraints (From Failures)

This section is updated when agents discover new constraints through failures.

| Constraint | Discovered | Run ID | Evidence |
|------------|------------|--------|----------|
| ArduinoJson deprecation warnings are safe to ignore | 2025-12-12 | worker-002 | Build succeeds with warnings |
| On ESP32-S3 FastLED RMT4 (`ESP-IDF 4.4.x`), `FASTLED_RMT_MAX_CHANNELS` must be `4` when `FASTLED_RMT_MEM_BLOCKS=2` to enable channel `0` + `2` parallel output | 2026-02-28 | manual-rmt-bottleneck | Live logs: `show` reduced from ~11.16ms to ~6.31ms after setting `4` |

---

## Source Files

- `src/config/hardware_config.h` - Hardware constants
- `src/config/features.h` - Feature flags
- `src/config/network_config.h` - Network settings
- `platformio.ini` - Build configuration
- `docs/architecture/00_LIGHTWAVEOS_INFRASTRUCTURE_COMPREHENSIVE.md` - Full architecture
