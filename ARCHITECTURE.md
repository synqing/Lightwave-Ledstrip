# Architecture Quick Reference (For Agents)

This file contains extracted key facts. For full details, see `docs/architecture/`.

---

## System Identity

| Property | Value | Source |
|----------|-------|--------|
| MCU | ESP32-S3 @ 240MHz, dual-core | platformio.ini |
| Flash | 8MB | platformio.ini |
| RAM | 320KB SRAM | ESP32-S3 spec |
| LEDs | 320 total (2 x 160 WS2812) | hardware_config.h |
| Target FPS | 120 (achieving 176) | docs/architecture/00_* |
| Frame Budget | 5.68ms @ 176 FPS | docs/architecture/00_* |

---

## Critical Invariants (NEVER VIOLATE)

### 1. CENTER ORIGIN
All effects MUST originate from LED 79/80 (center point).

**Valid patterns:**
- Outward: 79/80 → 0 and 79/80 → 159
- Inward: 0/159 → 79/80

**Invalid patterns:**
- Linear left-to-right (0 → 159)
- Linear right-to-left (159 → 0)
- Random origin points

**Why:** The Light Guide Plate creates interference patterns. Edge-originating effects look wrong on this hardware.

### 2. NO RAINBOWS
Rainbow color cycling is explicitly forbidden. Will be rejected.

### 3. NO MALLOC IN RENDER LOOPS
Heap allocation in effect render functions causes frame drops.

**Valid:**
```cpp
static CRGB buffer[160];  // Static allocation
```

**Invalid:**
```cpp
CRGB* buffer = new CRGB[160];  // Dynamic in render loop
```

---

## Hardware Pinout

| Function | GPIO | Notes |
|----------|------|-------|
| Strip 1 Data | 4 | WS2812, 160 LEDs |
| Strip 2 Data | 5 | WS2812, 160 LEDs |
| I2C SDA | 17 | M5Stack 8encoder |
| I2C SCL | 18 | M5Stack 8encoder |

Source: `src/config/hardware_config.h`

---

## LED Buffer Layout

```
Strip 1: leds[0..159]     Strip 2: leds[160..319]
         ↓                         ↓
    ┌────────────────┐        ┌────────────────┐
    │ 0 ←── 79/80 ──→ 159│    │160←── 239/240 ──→319│
    └────────────────┘        └────────────────┘
         CENTER                     CENTER
```

Global buffer: `CRGB leds[320]`

---

## Data Flow

```
┌─────────────────────────────────────────────────────────────┐
│                        INPUT LAYER                          │
│  Web Interface (data/*.html/js/css)                         │
│  Serial Menu (115200 baud)                                  │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                      CONTROL LAYER                          │
│  WebServer (src/network/WebServer.cpp)                      │
│  REST API: /api/effect, /api/brightness, etc.               │
│  WebSocket: Real-time updates                               │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                      EFFECT LAYER                           │
│  Zone Composer (src/effects/zones/ZoneComposer.cpp)         │
│  Effect Functions (src/effects/strip/*.cpp)                 │
│  Transition Engine (src/effects/transitions/)               │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                      OUTPUT LAYER                           │
│  FastLED + RMT Driver                                       │
│  leds[320] → strip1[160] + strip2[160]                      │
│  WS2812 @ 800kHz, ~9.6ms for 320 LEDs                       │
└─────────────────────────────────────────────────────────────┘
```

---

## Key Files by Task

### Adding a New Effect
1. Create function in `src/effects/strip/`
2. Register in `effects[]` array in `src/main.cpp` (~line 202)
3. Follow CENTER ORIGIN pattern
4. Test with serial menu (`e` command)

### Modifying Web Interface
1. Edit `data/index.html`, `data/app.js`, `data/styles.css`
2. Build: `pio run -e esp32dev_wifi -t upload`
3. Access: `http://lightwaveos.local`

### Adding Zone Effects
1. Effect must work with buffer proxy (temp buffer)
2. Zone Composer calls effect, then composites
3. Don't hardcode LED indices

---

## Build Environments

| Environment | Command | Features |
|-------------|---------|----------|
| esp32dev | `pio run` | Default, no WiFi |
| esp32dev_wifi | `pio run -e esp32dev_wifi` | WiFi + WebServer |
| memory_debug | `pio run -e memory_debug` | Heap tracing |

---

## Effect Registration Pattern

```cpp
// In main.cpp
Effect effects[] = {
    {"Effect Name", effectFunction, EFFECT_TYPE_STANDARD},
    // ...
};

// Effect function signature
void effectFunction() {
    // Access globals: leds[], currentPalette, gHue, fadeAmount, brightnessVal
    // MUST use CENTER ORIGIN pattern
}
```

---

## For Full Architecture Details

See: `docs/architecture/00_LIGHTWAVEOS_INFRASTRUCTURE_COMPREHENSIVE.md`

This document has:
- Mermaid diagrams of all subsystems
- Timing breakdowns
- Memory architecture
- Network stack details
- Performance optimization techniques
