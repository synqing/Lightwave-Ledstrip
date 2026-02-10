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

### 4. DEFENSIVE BOUNDS CHECKING
All array accesses MUST be validated before use to prevent LoadProhibited crashes.

**Valid:**
```cpp
uint8_t safeId = validateEffectId(effectId);  // Validate first
m_effects[safeId].active = true;               // Then use
```

**Invalid:**
```cpp
m_effects[effectId].active = true;  // BAD: No validation!
```

**Why:** Memory corruption, race conditions, or invalid input can cause indices to be out of bounds, leading to system crashes.

**Pattern:** All validation functions follow `validateXxx()` naming and return safe defaults (typically 0 for indices).

See `v2/docs/architecture/DEFENSIVE_BOUNDS_CHECKING.md` for full documentation.

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
│  Effect Classes (src/effects/ieffect/*.cpp)                 │
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
1. Create class implementing `IEffect` in `src/effects/ieffect/`
2. Register in `registerAllEffects()` in `src/effects/CoreEffects.cpp`
3. Follow CENTER ORIGIN pattern
4. Test with serial menu (`e` command) or web UI

### Modifying Web Interface
1. Edit `data/index.html`, `data/app.js`, `data/styles.css`
2. Build: `pio run -e esp32dev_audio_esv11 -t upload`
3. Access: `http://lightwaveos.local`

### Adding Zone Effects
1. Effect must work with buffer proxy (temp buffer)
2. Zone Composer calls effect, then composites
3. Don't hardcode LED indices

---

## Build Environments

| Environment | Command | Features |
|-------------|---------|----------|
| esp32dev_audio_esv11 | `pio run -e esp32dev_audio_esv11` | Primary build: WiFi + Audio + WebServer |
| esp32dev_audio_benchmark | `pio run -e esp32dev_audio_benchmark` | Audio pipeline benchmarking |
| esp32dev_audio_trace | `pio run -e esp32dev_audio_trace` | Perfetto timeline tracing |
| esp32dev_SSB | `pio run -e esp32dev_SSB` | Sensory Bridge compatibility |
| native | `pio test -e native` | Native unit tests (no hardware) |

---

## Effect Registration Pattern

```cpp
// In src/effects/CoreEffects.cpp
void registerAllEffects(RendererActor* renderer) {
    renderer->registerEffect(0, "Fire", std::make_unique<FireEffect>());
    renderer->registerEffect(1, "Ocean", std::make_unique<OceanEffect>());
    // ... 101+ effects registered with stable IDs
}

// Effect class (src/effects/ieffect/MyEffect.h)
class MyEffect : public IEffect {
public:
    void init(const EffectContext& ctx) override { /* setup */ }
    void render(const EffectContext& ctx, CRGB* leds, uint16_t numLeds) override {
        // MUST use CENTER ORIGIN pattern
    }
    const char* name() const override { return "MyEffect"; }
    EffectCategory category() const override { return EffectCategory::LGP_INTERFERENCE; }
};
```

---

## Common Pitfalls

### Array Access Without Validation
**Problem:** Direct array access without bounds checking can cause LoadProhibited crashes.

**Solution:** Always validate indices before array access:
```cpp
uint8_t safeId = validateEffectId(effectId);
m_effects[safeId].active = true;
```

See `v2/docs/architecture/DEFENSIVE_BOUNDS_CHECKING.md` for the validation pattern.

### Palette Staleness
**Problem:** Caching palette pointers across frames can lead to stale data.

**Solution:** Fetch palette reference per frame, don't cache pointers.

### Centre Mapping Errors
**Problem:** Centre is between LEDs 79 and 80, not at LED 80.

**Solution:** Use `CENTER_LEFT` (79) and `CENTER_RIGHT` (80) constants from `CoreEffects.h`.

---

## For Full Architecture Details

See: `docs/architecture/00_LIGHTWAVEOS_INFRASTRUCTURE_COMPREHENSIVE.md`

This document has:
- Mermaid diagrams of all subsystems
- Timing breakdowns
- Memory architecture
- Network stack details
- Performance optimization techniques

## Defensive Programming

LightwaveOS v2 implements comprehensive defensive bounds checking to prevent memory access violations. All array accesses must be validated before use.

**Key Documents:**
- `v2/docs/architecture/DEFENSIVE_BOUNDS_CHECKING.md` - Validation pattern guide
- `v2/docs/performance/VALIDATION_OVERHEAD.md` - Performance impact analysis
