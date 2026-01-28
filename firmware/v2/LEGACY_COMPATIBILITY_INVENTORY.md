# Legacy Compatibility Inventory
**Phase 0 Audit Output - Forward-Port Planning**

**Date:** 2026-01-28
**Purpose:** Complete inventory of v1 dependencies required to run legacy effects in v2

---

## 1. v2 Build Targets

### Firmware Builds
- **`esp32dev_audio`** (primary): ESP32-S3 firmware with WiFi, Audio, and WebServer
- **`esp32dev_audio_benchmark`**: Audio pipeline benchmarking
- **`esp32dev_audio_trace`**: Perfetto timeline tracing
- **`esp32dev_SSB`**: Sensory Bridge compatibility build

### Test Builds
- **`native_test`**: Native unit tests (no hardware, uses mocks)

**Status:** v2 currently compiles for ESP32-S3 target (confirmed via platformio.ini)

---

## 2. v1 Global Variables Required by Legacy Effects

### LED Buffers
```cpp
CRGB leds[320];                    // Unified buffer (EffectBase compatibility)
CRGB strip1[160];                  // Strip 1 buffer
CRGB strip2[160];                  // Strip 2 buffer
CRGB transitionBuffer[320];        // Transition system buffer
CRGB transitionSourceBuffer[320];  // Transition source buffer
```

### Animation Parameters
```cpp
uint8_t gHue = 0;                  // Global hue offset (0-255)
uint8_t brightnessVal = 96;        // Brightness level (0-255)
uint8_t effectSpeed = 10;          // Animation speed (1-50)
uint8_t paletteSpeed = 10;         // Palette animation speed
```

### Visual Parameters
```cpp
uint8_t effectIntensity = 255;     // Effect intensity (0-255)
uint8_t effectSaturation = 255;    // Color saturation (0-255)
uint8_t effectComplexity = 128;    // Effect complexity (0-255)
uint8_t effectVariation = 128;     // Effect variation (0-255)
VisualParams visualParams;         // Structured visual params
```

### Palette System
```cpp
CRGBPalette16 currentPalette;      // Current active palette
CRGBPalette16 targetPalette;       // Target palette (for transitions)
uint8_t currentPaletteIndex = 0;   // Index into master palette array
bool paletteAutoCycle = true;      // Auto-cycle toggle
uint32_t paletteCycleInterval = 5000; // Cycle interval (ms)
```

### Effect State
```cpp
uint8_t currentEffect = 0;         // Current effect ID
uint8_t previousEffect = 0;        // Previous effect ID
```

### Hardware Configuration Constants
```cpp
// From HardwareConfig namespace:
HardwareConfig::STRIP_LENGTH        // 160
HardwareConfig::STRIP_CENTER_POINT  // 79
HardwareConfig::STRIP_HALF_LENGTH   // 80
HardwareConfig::STRIP1_LED_COUNT    // 160
HardwareConfig::STRIP2_LED_COUNT    // 160
HardwareConfig::NUM_LEDS            // 320
HardwareConfig::TOTAL_LEDS          // 320
```

### Master Palette System
```cpp
extern const TProgmemRGBGradientPaletteRef gMasterPalettes[];
extern const uint8_t gMasterPaletteCount;
extern const char* MasterPaletteNames[];
```

### Transition System
```cpp
bool useRandomTransitions = true;  // Random transition toggle
```

### Strip Mapping Arrays
```cpp
uint8_t angles[320];               // Angle mapping (for spatial effects)
uint8_t radii[320];                // Radius mapping (for spatial effects)
```

---

## 3. Helper Headers Required by Legacy Effects

### Core Headers
- **`config/hardware_config.h`**: Hardware constants (STRIP_LENGTH, CENTER_POINT, etc.)
- **`core/EffectTypes.h`**: VisualParams struct, EasingCurve enum, NarrativePhase enum
- **`Palettes_Master.h`**: Master palette system declarations

### Utility Headers
- **`utils/PerformanceHacks.h`**: Performance macros (ALWAYS_INLINE, HOT_FUNCTION, NoInterrupts, UltraFastPixelOps)
- **`utils/OptimizedFastLED.h`**: Optimized FastLED operations (show(), clear(), fill(), fadeToBlack(), setBrightness(), blend())
- **`utils/TrigLookup.h`**: Trigonometric lookup tables (sin16, cos16, etc.)
- **`effects/utils/FastLEDOptim.h`**: FastLED optimization utilities (fastScaleRGB, radiansToPhase16, etc.)

### Standard Libraries
- **`<FastLED.h>`**: FastLED library (CRGB, CHSV, ColorFromPalette, etc.)
- **`<Arduino.h>`**: Arduino core (millis(), micros(), random8(), etc.)
- **`<math.h>`**: Math functions (sin, cos, abs, fmod, etc.)

---

## 4. Effect-to-Effect Dependencies

### Shared Helper Functions
Some effects call helper functions defined in other effect files. These must be available:

- **FastLED optimization helpers**: Used across LGP effects
  - `FastLEDOptim::fastScaleRGB()`
  - `FastLEDOptim::radiansToPhase16()`
  - `FastLEDOptim::fastSin16()`
  - `FastLEDOptim::fastCos16()`

- **Hardware config constants**: Used by all strip effects
  - `HardwareConfig::STRIP_CENTER_POINT`
  - `HardwareConfig::STRIP_LENGTH`
  - `HardwareConfig::STRIP_HALF_LENGTH`

### No Direct Function Calls Between Effects
Effects are stateless `void(*)()` functions - they don't call each other directly. All dependencies are via globals and helper headers.

---

## 5. Directory Layout for v2 Legacy Payload

### Proposed Structure
```
v2/src/plugins/legacy/
├── v1_globals/
│   ├── V1Globals.h          # Declarations for all v1 globals
│   └── V1Globals.cpp        # Definitions and initialization
├── v1_compat/
│   ├── hardware_config_compat.h  # Shim for HardwareConfig namespace
│   └── effect_types_compat.h     # Shim for EffectTypes if needed
└── v1_effects/
    ├── basic/               # Basic effects (copied from src/effects/basic/)
    ├── strip/               # Strip effects (copied from src/effects/strip/)
    ├── lightguide/          # Lightguide effects (copied from src/effects/lightguide/)
    ├── advanced/            # Advanced effects (copied from src/effects/advanced/)
    └── plugins/             # Plugin effects (copied from src/effects/plugins/)
```

### Effect Count
- **Total effect header files**: ~103 files in `src/effects/ieffect/`
- **Registered effects**: 101 (EXPECTED_EFFECT_COUNT in CoreEffects.cpp)
- **MAX_EFFECTS ceiling**: 104 (room for growth)
- **Registration**: `registerAllEffects()` in `CoreEffects.cpp` using `IEffect` interface

---

## 6. v2 Effect Registration System

### Current v2 Registration
- **Entry point**: `v2/src/effects/CoreEffects.cpp::registerAllEffects(RendererActor*)`
- **Registration method**: `RendererActor::registerEffect(uint8_t id, const char* name, EffectRenderFn fn)`
- **Function signature**: `void (*EffectRenderFn)(RenderContext& ctx)`
- **Current v2 effects**: Use `RenderContext&` parameter

### Legacy Effect Signature
- **v1 effects**: `void (*LegacyEffectFunc)()` - no parameters
- **Adapter**: `LegacyEffectAdapter` bridges `EffectContext` → v1 globals → legacy function

### Registration Strategy
1. Wrap each legacy `void(*)()` function with `LegacyEffectAdapter`
2. Create `EffectMetadata` for each effect (name, description, category)
3. Register via `RendererActor::registerEffect()` with stable IDs
4. Maintain ID mapping: legacy effect order must match UI/API expectations

---

## 7. Feature Parity Acceptance Criteria

### Effects
- [ ] All 101+ effects render correctly in v2
- [ ] At least 5 representative effects verified (including one heavy LGP effect)
- [ ] No crashes from missing globals/palettes
- [ ] Effect IDs match between v1 and v2 (for API compatibility)

### PatternRegistry
- [ ] All effects (native + legacy) registered in PatternRegistry
- [ ] Registry can enumerate effects by category/tags
- [ ] Metadata accessible via API/debug

### Narrative Engine
- [ ] Single NarrativeEngine provides both phase machine and tension curves
- [ ] Tension values (0-1) available to effects
- [ ] Tempo multiplier functional
- [ ] Phase transitions work correctly

### ShowDirector
- [ ] Shows can be loaded and started
- [ ] Cues execute at correct times
- [ ] Parameter sweeps work
- [ ] Show-driven effect/parameter changes flow through MessageBus/CQRS

### Utilities
- [ ] FastLED utilities consolidated in v2/utils/
- [ ] No duplicate/conflicting helper functions
- [ ] Legacy effects compile with v2 utility layer

---

## 8. Edge Cases and Special Considerations

### Center-Origin Compliance
- All effects must respect CENTER ORIGIN (LEDs 79/80)
- v1 effects already comply (enforced in v1)
- v2 must maintain this constraint

### Palette System
- v1 uses `CRGBPalette16 currentPalette` (global)
- v2 uses `gMasterPalettes[]` (PROGMEM array)
- Bridge: `V1Globals` must sync `currentPalette` from v2's active palette

### Dual-Strip Handling
- v1 effects write to `strip1[]` and `strip2[]` separately
- v2 uses unified `leds[320]` buffer
- Bridge: `V1Globals` must provide both views (strip1/strip2 as views into unified buffer)

### Performance
- Legacy effects are called via adapter (minimal overhead)
- Adapter overhead: ~10-20 instructions per frame (negligible)
- No performance regression expected

### Memory
- v1 globals: ~2KB RAM (LED buffers + state)
- v2 already has LED buffers in RendererActor
- Bridge: `V1Globals` can reference v2's buffers (no duplication)

---

## 9. Dependencies Summary

### Must Copy to v2
1. **Effect source files** (~74 files from `src/effects/`)
2. **Utility headers** (`utils/PerformanceHacks.h`, `utils/OptimizedFastLED.h`, `utils/TrigLookup.h`, `effects/utils/FastLEDOptim.h`)
3. **Config headers** (`config/hardware_config.h` - or create shim)
4. **Type headers** (`core/EffectTypes.h` - or create shim)
5. **Palette system** (`Palettes_Master.h` - v2 already has this)

### Must Create in v2
1. **`V1Globals.h/.cpp`**: All v1 global variable definitions
2. **Shim headers**: Compatibility layer for HardwareConfig/EffectTypes if needed
3. **Registration function**: `registerLegacyV1Effects(RendererActor*)`

### Must Update in v2
1. **`LegacyEffectAdapter.h`**: Bridge to v2's `V1Globals` instead of root `src/main.cpp`
2. **`CoreEffects.cpp`**: Call `registerLegacyV1Effects()` after native effects

---

## 10. Risk Assessment

### Low Risk
- ✅ Effect functions are stateless - no hidden dependencies
- ✅ All globals are explicitly declared in `src/main.cpp`
- ✅ Helper headers are well-defined
- ✅ v2 already has `LegacyEffectAdapter` designed for this

### Medium Risk
- ⚠️ ID mapping must be stable (test with native tests)
- ⚠️ Palette sync between v2 state and v1 globals
- ⚠️ Dual-strip buffer views (strip1/strip2 as views into unified buffer)

### Mitigation
- Create explicit ID mapping table (tested)
- Implement palette sync in `V1Globals` update function
- Use pointer aliases for strip1/strip2 (no memory duplication)

---

**Phase 0 Complete:** All dependencies identified, directory layout defined, parity criteria established.

**Next:** Phase 1 - Implement `V1Globals` and copy legacy effects into v2.

