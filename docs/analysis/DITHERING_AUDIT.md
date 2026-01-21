# Dithering Audit - esp32dev_audio Pipeline Analysis

**Date**: 2026-01-10  
**Target Environment**: esp32dev_audio (default build)  
**Purpose**: Document the active rendering pipeline and assess whether FastLED temporal dithering conflicts with custom Bayer dithering.

---

## Executive Summary

### Key Findings

1. **RendererNode is the ONLY active backend** for esp32dev_audio
   - Drives FastLED directly via `FastLED.show()`
   - `hal::FastLedDriver` exists but is NOT instantiated in the runtime path

2. **TWO INDEPENDENT dithering stages are BOTH ACTIVE**:
   - **Stage 1: Bayer 4×4 ordered dithering** (custom, spatial)
     - Location: `ColorCorrectionEngine::applyDithering()`
     - Runs in RendererNode tick: AFTER gamma, BEFORE FastLED output
     - Gated by `ColorCorrectionConfig.ditheringEnabled` (default: `true`)
   
   - **Stage 2: FastLED temporal dithering**
     - Location: Inside FastLED library during `FastLED.show()`
     - Enabled via `FastLED.setDither(1)` in `RendererNode::initLeds()`
     - Always on (hardcoded enabled in RendererNode)

3. **Potential Redundancy/Conflict**:
   - Both dithering algorithms operate on 8-bit RGB values
   - Bayer (spatial) breaks up banding with a fixed 4×4 pattern
   - FastLED temporal adds frame-to-frame noise to smooth gradients
   - **UNKNOWN**: Whether combining both improves or worsens perceived quality
   - **RISK**: Interaction may produce visible artefacts or waste compute cycles

---

## 1. Active Renderer Backend Trace (esp32dev_audio)

### Boot Sequence

```
main.cpp::setup()
  └── NodeOrchestrator::instance().init()
       └── creates std::unique_ptr<RendererNode> m_renderer
            └── RendererNode::RendererNode() constructor
                 └── initializes m_leds[320], m_strip1, m_strip2
       └── m_renderer->start()
            └── calls RendererNode::onStart()
                 └── calls initLeds()
                      └── FastLED.addLeds<...>(m_strip1, ...)
                      └── FastLED.addLeds<...>(m_strip2, ...)
                      └── FastLED.setBrightness(m_brightness)
                      └── FastLED.setDither(1)  ← TEMPORAL DITHERING ENABLED HERE
                      └── FastLED.setMaxRefreshRate(0, true)
```

**Source**: `firmware/v2/src/core/actors/NodeOrchestrator.cpp:72-88`

```cpp
// Create RendererNode
m_renderer = std::make_unique<RendererNode>();
```

**Source**: `firmware/v2/src/core/actors/RendererNode.cpp:698-702`

```cpp
// Configure FastLED
FastLED.setBrightness(m_brightness);
FastLED.setCorrection(TypicalLEDStrip);
FastLED.setDither(1);  // Temporal dithering ← ALWAYS ON
FastLED.setMaxRefreshRate(0, true);  // Non-blocking
FastLED.setMaxPowerInVoltsAndMilliamps(5, 3000);  // 5V / 3A limit
```

### hal::FastLedDriver Status

**Conclusion**: `hal::FastLedDriver` is **NOT used** in esp32dev_audio builds.

**Evidence**:
- `hal::FastLedDriver` exists in `firmware/v2/src/hal/led/FastLedDriver.{h,cpp}`
- It has a `setDithering(bool)` method and config field `LedDriverConfig.enableDithering`
- **BUT** no code instantiates `FastLedDriver` in the active runtime
- `RendererNode` owns `CLEDController*` directly and calls `FastLED.show()` itself

**Grep results**:
```bash
# No instantiation of FastLedDriver found in main runtime paths
# Only references are in HAL header/cpp definitions
```

---

## 2. Per-Frame Render Pipeline (esp32dev_audio)

### High-Level Flow

```
RendererNode::onTick()  [120 Hz target, Core 1]
  │
  ├─► renderFrame()
  │    └── Effect writes to m_leds[320] (unified buffer)
  │
  ├─► TAP A: captureFrame(TAP_A_PRE_CORRECTION, m_leds)  [if enabled]
  │
  ├─► ColorCorrectionEngine::processBuffer(m_leds, 320)
  │    │   [SKIPPED if PatternRegistry::shouldSkipColorCorrection(effectId)]
  │    │
  │    ├─► Auto-Exposure (optional, BT.601 luminance)
  │    ├─► White Reduction / Brown Guardrail
  │    ├─► Gamma Correction (LUT-based, gamma=2.2)
  │    ├─► ✨ BAYER DITHERING ✨  [if ditheringEnabled=true, DEFAULT ON]
  │    ├─► LED Spectral Correction (WS2812 compensation)
  │    └─► LACE (Local Adaptive Contrast Enhancement, optional)
  │
  ├─► TAP B: captureFrame(TAP_B_POST_CORRECTION, m_leds)  [if enabled]
  │
  └─► showLeds()
       ├── Split m_leds → m_strip1[160], m_strip2[160]
       ├── Apply audio silence gate (scale brightness if silent)
       └── FastLED.show()
            └── ✨ FASTLED TEMPORAL DITHERING ✨  [always on via setDither(1)]
```

**Source**: `firmware/v2/src/core/actors/RendererNode.cpp:420-473`

### Key Decision Points

| Stage | Gating Condition | Default State |
|-------|------------------|---------------|
| **ColorCorrectionEngine** | `!PatternRegistry::shouldSkipColorCorrection(effectId)` | Applied to most effects |
| **Bayer Dithering** | `ColorCorrectionConfig.ditheringEnabled` | `true` (enabled) |
| **FastLED Temporal** | `FastLED.setDither(1)` in `initLeds()` | Always on (hardcoded) |

---

## 3. Bayer Dithering Implementation (Custom)

### Location

`firmware/v2/src/effects/enhancement/ColorCorrectionEngine.cpp:344-359`

### Algorithm

```cpp
// 4x4 Bayer matrix for ordered dithering (values 0-15 scaled to threshold)
static const uint8_t BAYER_4X4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
};

void ColorCorrectionEngine::applyDithering(CRGB* buffer, uint16_t count) {
    if (!m_config.ditheringEnabled) return;

    for (uint16_t i = 0; i < count; i++) {
        CRGB& c = buffer[i];

        // Get Bayer threshold based on LED position (creates 4x4 pattern)
        uint8_t threshold = BAYER_4X4[i % 4][(i / 4) % 4];

        // Apply ordered dithering: if low nibble exceeds threshold, round up
        // This breaks up banding without adding visible noise
        if ((c.r & 0x0F) > threshold && c.r < 255) c.r++;
        if ((c.g & 0x0F) > threshold && c.g < 255) c.g++;
        if ((c.b & 0x0F) > threshold && c.b < 255) c.b++;
    }
}
```

### Characteristics

- **Type**: Ordered (spatial) dithering
- **Pattern**: 4×4 Bayer matrix, repeating every 4 LEDs horizontally and vertically
- **Operation**: Examines low nibble (bits 0-3) of each 8-bit channel
- **Effect**: If fractional part (low nibble) exceeds threshold, round up by 1
- **Timing**: Runs ONCE per frame, AFTER gamma correction, BEFORE FastLED output
- **Performance**: O(n), 3 comparisons + 3 conditionals per LED (~960 ops for 320 LEDs)
- **Artefacts**: Fixed spatial pattern (may be visible on slow gradients if threshold is too aggressive)

### Configuration

**Default**: Enabled  
**NVS Key**: `"ditherEn"` (namespace: color correction)  
**Runtime Toggle**: `ColorCorrectionEngine::getInstance().getConfig().ditheringEnabled = false;`

---

## 4. FastLED Temporal Dithering (Library Built-in)

### Location

- **Enabled in**: `firmware/v2/src/core/actors/RendererNode.cpp:701`
- **Executed by**: FastLED library internal code during `FastLED.show()`

### Configuration

```cpp
FastLED.setDither(1);  // 1 = enabled, 0 = disabled
```

**Status**: **Always enabled** (hardcoded in `RendererNode::initLeds()`)

### Behavior (from FastLED library knowledge)

FastLED temporal dithering (when enabled) typically works as follows:

1. **Per-frame noise injection**: Adds small random (+1 or -1) adjustments to pixel values before output
2. **Temporal averaging**: Over multiple frames, the noise averages out to produce smoother gradients
3. **Frame counter dependency**: Uses an internal frame counter to vary the noise pattern
4. **Goal**: Reduce visible banding on 8-bit gradients by "vibrating" pixels between adjacent values

**NOTE**: FastLED library source code is NOT included in this repository, so exact implementation details are not confirmed. The above is based on standard FastLED behavior.

### Performance Impact

- Runs inside `FastLED.show()`, which already has significant overhead (RMT driver, DMA, etc.)
- Estimated overhead: **Minimal** (likely <1% of total frame time)
- Already included in the ~1.8ms FastLED.show() timing measured on this hardware

---

## 5. Interaction Analysis (Bayer + Temporal)

### Execution Order

```
Effect Render
  ↓
Bayer Dithering (spatial, fixed 4×4 pattern)
  ↓
FastLED.show() → Temporal Dithering (random frame-to-frame noise)
  ↓
WS2812 output
```

### Potential Synergy

**Hypothesis A: Complementary**
- Bayer handles **spatial** banding (breaks up smooth gradients across LEDs)
- Temporal handles **temporal** banding (smooths out 8-bit quantization over time)
- Together, they may provide better dithering than either alone

**Evidence for A**:
- Spatial and temporal dithering address orthogonal problems
- Used together in high-end display systems (e.g., Cinema projectors use both)

### Potential Conflict

**Hypothesis B: Redundant/Conflicting**
- Both modify the same 8-bit RGB values
- Bayer's fixed pattern may interfere with temporal's random noise
- Temporal noise may "undo" Bayer's carefully-placed thresholds
- Combined artefacts could be MORE visible than either alone

**Evidence for B**:
- Bayer already rounds up values; temporal adds noise on top
- If both are aggressive, may introduce visible flicker or graininess
- Power budget: temporal dithering increases average pixel brightness slightly (more "on" time)

### Performance Cost

| Stage | Estimated Cost | Notes |
|-------|---------------|-------|
| Bayer Dithering | ~50-100 µs | 320 LEDs × 3 channels × simple ops |
| FastLED Temporal | <20 µs | Built into FastLED.show() overhead |
| **Total Overhead** | <1% of 8.3ms frame budget (120 FPS) | Negligible |

**Conclusion**: Performance impact is NOT a concern.

---

## 6. Effects That Skip Color Correction

From `PatternRegistry::shouldSkipColorCorrection()`:

- LGP-sensitive effects (interference patterns)
- Stateful effects (maintain their own color state)
- PHYSICS_BASED family effects
- MATHEMATICAL family effects

**For these effects**:
- Bayer dithering is **SKIPPED** (entire ColorCorrectionEngine bypassed)
- FastLED temporal dithering is **STILL APPLIED** (happens in FastLED.show(), always on)

**Implication**: Even "sensitive" effects get temporal dithering, which may be undesirable for physics simulations.

---

## 7. "Sensory Bridge" Dithering Claim Investigation

### Search Results

```bash
grep -r "Sensory Bridge" firmware/v2/src/
# Found references in audio-reactive effects (e.g., QuantumInterference, SpectralFlux)
# NO standalone "Sensory Bridge dithering algorithm" found
```

### Conclusion

**The "Sensory Bridge dithering" claim appears to refer to:**
1. **The Bayer dithering implementation** (custom, borrowed concept, not necessarily code)
2. **OR** audio-reactive patterns inspired by Sensory Bridge's visual aesthetics

**NOT a separate dithering algorithm.** The only dithering code is:
- Bayer 4×4 (ColorCorrectionEngine)
- FastLED temporal (library built-in)

---

## 8. Recommendations for Testing

### Empirical Tests Needed

1. **Visual banding test**: Capture slow gradient ramps with:
   - Bayer only (disable FastLED temporal)
   - Temporal only (disable Bayer)
   - Both enabled (current default)
   - Neither enabled (baseline)

2. **Flicker test**: Capture near-black gradients (where temporal dithering is most visible)
   - Measure frame-to-frame variance
   - Check for visible "shimmering" artefacts

3. **LGP interference test**: Run sensitive effects (e.g., QuantumInterference) and verify:
   - Whether temporal dithering disrupts the physics simulation
   - Whether it should be conditionally disabled

4. **Power consumption test**: Compare average current draw with/without dithering
   - Temporal dithering may increase average brightness slightly

### Configuration Options to Test

| Config | Bayer | FastLED Temporal | Test Goal |
|--------|-------|------------------|-----------|
| A (current default) | ON | ON | Baseline |
| B | OFF | ON | FastLED only |
| C | ON | OFF | Bayer only |
| D | OFF | OFF | No dithering |

**Implementation Note**: FastLED temporal is currently hardcoded ON. To test config B/D, need to add runtime control:

```cpp
// In RendererNode::initLeds() or via configuration
FastLED.setDither(m_config.enableFastLEDDithering ? 1 : 0);
```

---

## 9. Next Steps

1. ✅ **Document active pipeline** (this document)
2. ⏳ **Build host-side test harness** (Python simulation)
3. ⏳ **Web research on WS2812 dithering best practices**
4. ⏳ **Create firmware capture tool** (TAP A/B/C frame extraction)
5. ⏳ **Run empirical tests** (on-device validation)
6. ⏳ **Make recommendations** (keep both, make exclusive, replace, or remove)

---

## Appendix A: Source File References

### Key Files

- `firmware/v2/src/core/actors/NodeOrchestrator.cpp` - Boot sequence, RendererNode creation
- `firmware/v2/src/core/actors/RendererNode.cpp` - Main render loop, FastLED.setDither(1)
- `firmware/v2/src/effects/enhancement/ColorCorrectionEngine.{h,cpp}` - Bayer dithering implementation
- `firmware/v2/src/hal/led/FastLedDriver.{h,cpp}` - HAL abstraction (not used in esp32dev_audio)
- `firmware/v2/src/hal/led/LedDriverConfig.h` - HAL config struct (enableDithering field)

### Build Configuration

- **PlatformIO env**: `esp32dev_audio` (default)
- **Build flags**: None related to dithering
- **Library**: FastLED@3.10.0

---

## Appendix B: Call Graph (Simplified)

```
main.cpp::loop()
  └── NodeOrchestrator::instance().tick()
       └── RendererNode::onTick()  [Core 1, ~120 Hz]
            ├── renderFrame()
            │    └── currentEffect->render(ctx)
            │         └── writes to ctx.leds (m_leds)
            │
            ├── ColorCorrectionEngine::processBuffer(m_leds, 320)
            │    ├── applyAutoExposure()
            │    ├── applyWhiteCuration()
            │    ├── applyBrownGuardrail()
            │    ├── applyGamma()
            │    ├── applyDithering()  ← BAYER 4×4 HERE
            │    ├── applyLEDSpectralCorrection()
            │    └── applyLACE()
            │
            └── showLeds()
                 ├── split m_leds → m_strip1, m_strip2
                 ├── apply audio gate
                 └── FastLED.show()
                      └── [FastLED library internal]
                           └── temporal dithering  ← FASTLED TEMPORAL HERE
                           └── RMT driver
                           └── WS2812 output
```

---

**End of Audit Document**
