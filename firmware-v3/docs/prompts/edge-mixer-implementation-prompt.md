# EdgeMixer Implementation — One-Shot CC Agent Prompt

## Objective

Implement a dual-edge hue-splitting post-processor (`EdgeMixer`) for the LightwaveOS LED rendering pipeline. This transforms the Light Guide Plate (LGP) from a flat mirror (both strips identical) into a colour-differentiated depth field (each strip gets a harmonically related colour variant).

## Architecture Context

**Platform:** ESP32-S3, 320 WS2812 LEDs (2×160), 120 FPS render loop, FreeRTOS actor model.

**Render pipeline (current):**
```
renderFrame()           → effects write to m_leds[320]
ColorCorrection         → processBuffer(m_leds, 320)
showLeds():
  ├─ Tone mapping       → optional Reinhard LUT
  ├─ Strip split        → memcpy m_leds[0..159] → m_strip1
  │                       memcpy m_leds[160..319] → m_strip2
  ├─ Silence gates      → scale8() combined pass (FEATURE_AUDIO_SYNC only)
  ├─ TAP C capture      → diagnostic snapshot
  └─ m_ledDriver.show() → FastLED.show() → dual RMT → WS2812
```

**After this implementation:**
```
showLeds():
  ├─ Tone mapping
  ├─ Strip split        → memcpy to m_strip1, m_strip2
  ├─ Silence gates
  ├─ *** EdgeMixer ***  → hue-shift m_strip2 in-place
  ├─ TAP C capture
  └─ m_ledDriver.show()
```

## Hard Constraints (MANDATORY — violating any of these is a build-breaking error)

1. **No heap allocation in render path.** No `new`, `malloc`, `String`, or any dynamic allocation in `render()`, `showLeds()`, or any function called from them. Use static buffers only.
2. **120 FPS budget.** The EdgeMixer MUST complete in under 0.5ms for 160 pixels. The total frame budget is 8.33ms and ~1ms remains after existing processing.
3. **Centre origin.** All effects originate from LED 79/80 outward. The mixer does NOT change this — it operates on the already-rendered strip buffers.
4. **No rainbows.** No full hue-wheel sweeps. The hue shifts are deliberate harmonic offsets (analogous: ±15-30°, complementary: 180°).
5. **British English** in all comments, docs, and log messages: colour, centre, initialise, behaviour, etc.
6. **No `#include` of heavy headers in tight loops.** The mixer header must be lightweight.

## Files to Create

### 1. `src/effects/enhancement/EdgeMixer.h`

Header-only implementation. Follow the pattern of `ColorCorrectionEngine.h` (same directory) for namespace and style conventions.

```cpp
/**
 * @file EdgeMixer.h
 * @brief Dual-edge hue splitting for Light Guide Plate colour differentiation
 *
 * The EdgeMixer takes the rendered strip buffers and applies harmonic hue
 * shifts to Strip 2, creating perceived depth through the LGP. Three modes:
 *   - MIRROR: No processing (current behaviour, default)
 *   - ANALOGOUS: Shifts Strip 2 hue by ±spread/2 (warm/cool split)
 *   - COMPLEMENTARY: Shifts Strip 2 hue by 180° with saturation reduction
 *
 * Performance: Operates in-place on m_strip2[160]. Target: <0.5ms per frame.
 * Black pixel skip optimisation bypasses HSV conversion for (r|g|b)==0 pixels.
 */
```

**Namespace:** `lightwaveos::enhancement`

**Enum:**
```cpp
enum class EdgeMixerMode : uint8_t {
    MIRROR       = 0,  // No processing (default, current behaviour)
    ANALOGOUS    = 1,  // Hue shift ±spread/2 between edges
    COMPLEMENTARY = 2   // 180° hue shift with saturation reduction
};
```

**Class: `EdgeMixer`** — Singleton (follow `ColorCorrectionEngine` pattern).

**Public interface:**
```cpp
static EdgeMixer& getInstance();

// Apply mixer to strip buffer IN-PLACE. Only strip2 is modified.
// strip2 must point to LEDS_PER_STRIP (160) CRGB pixels.
void process(CRGB* strip2, uint16_t count);

// Configuration
void setMode(EdgeMixerMode mode);
EdgeMixerMode getMode() const;
void setSpread(uint8_t spread);    // Analogous hue spread in degrees (0-60, default 30)
uint8_t getSpread() const;
void setStrength(uint8_t strength); // Mix strength 0-255 (0=no effect, 255=full shift)
uint8_t getStrength() const;
```

**Private implementation:**

The `process()` method:
1. If mode == MIRROR, return immediately (zero overhead).
2. Iterate strip2[0..count-1]:
   - **Black pixel skip:** if `(pixel.r | pixel.g | pixel.b) == 0` → skip (critical optimisation).
   - Convert CRGB → CHSV using FastLED's `rgb2hsv_approximate()`.
   - **ANALOGOUS mode:** shift hue by `+spread/2` (uint8_t wrapping handles modular arithmetic naturally in FastLED's 0-255 hue space; spread of 30° ≈ value 21 in FastLED units where 255 = 360°).
   - **COMPLEMENTARY mode:** shift hue by `+128` (180° in FastLED units). Reduce saturation by 15% (`sat = scale8(sat, 217)`).
   - Apply strength blending: interpolate between original and shifted using `blend(originalCRGB, shiftedCRGB, strength)` or equivalent.
   - Convert back: `hsv2rgb_rainbow()` → write back to strip2[i].

**Important FastLED hue math:** FastLED uses 0-255 for 360°. So:
- 30° = ~21 in FastLED units (`30 * 256 / 360 ≈ 21`)
- 180° = 128 in FastLED units
- Hue wrapping is automatic with uint8_t overflow.

**NVS persistence (optional, low priority):** Store mode/spread/strength in Preferences. Follow `ColorCorrectionEngine` pattern. Key prefix: `"emix_"`.

## Files to Modify

### 2. `src/core/actors/RendererActor.h`

Add include at top (with other enhancement includes):
```cpp
#include "../../effects/enhancement/EdgeMixer.h"
```

No new member variables needed — EdgeMixer is a singleton accessed via `EdgeMixer::getInstance()`.

### 3. `src/core/actors/RendererActor.cpp`

**Insertion point:** Inside `showLeds()`, between the silence gates block (line ~1703, after the `#endif` closing `FEATURE_AUDIO_SYNC`) and the TAP C capture block (line ~1706).

Insert:
```cpp
    // Edge mixer: apply hue splitting to Strip 2 for LGP depth differentiation
    enhancement::EdgeMixer::getInstance().process(m_strip2, LedConfig::LEDS_PER_STRIP);
```

That's it. One line. The singleton handles mode checking internally, and MIRROR mode returns immediately with zero overhead.

### 4. `src/core/actors/Actor.h` — Add message type

In the `MessageType` enum, add in the effect commands range (0x00-0x1F). Use the next available slot:
```cpp
    SET_EDGE_MIXER_MODE = 0x0B,  // param1 = EdgeMixerMode (0=mirror, 1=analogous, 2=complementary)
    SET_EDGE_MIXER_SPREAD = 0x0C, // param1 = spread (0-60)
    SET_EDGE_MIXER_STRENGTH = 0x0D, // param1 = strength (0-255)
```

### 5. `src/core/actors/RendererActor.cpp` — Handle new messages

In `onMessage()`, add cases for the new message types (follow the existing `handleSetIntensity` etc. pattern):

```cpp
case MessageType::SET_EDGE_MIXER_MODE:
    enhancement::EdgeMixer::getInstance().setMode(
        static_cast<enhancement::EdgeMixerMode>(msg.param1));
    break;
case MessageType::SET_EDGE_MIXER_SPREAD:
    enhancement::EdgeMixer::getInstance().setSpread(msg.param1);
    break;
case MessageType::SET_EDGE_MIXER_STRENGTH:
    enhancement::EdgeMixer::getInstance().setStrength(msg.param1);
    break;
```

### 6. WebSocket command handler

Create `src/network/webserver/ws/WsEdgeMixerCommands.cpp` and `.h` following the pattern in `WsEffectsCommands.cpp`.

Handle these WebSocket commands:
- `edge_mixer.set` — `{ "action": "edge_mixer.set", "mode": 0|1|2, "spread": 0-60, "strength": 0-255 }`
- `edge_mixer.get` — returns current mode, spread, strength

Register the commands in `WsCommandRouter.cpp` (follow existing pattern for how commands are registered).

### 7. Status broadcast integration

In `WsDeviceCommands.cpp`, `handleLegacyGetStatus()` (line ~57-79), add to the status response:
```cpp
response["edgeMixerMode"] = static_cast<uint8_t>(enhancement::EdgeMixer::getInstance().getMode());
response["edgeMixerSpread"] = enhancement::EdgeMixer::getInstance().getSpread();
response["edgeMixerStrength"] = enhancement::EdgeMixer::getInstance().getStrength();
```

## File Structure Reference

```
firmware-v3/src/
├── core/actors/
│   ├── Actor.h                    ← MessageType enum (MODIFY)
│   ├── RendererActor.h            ← Add #include (MODIFY)
│   └── RendererActor.cpp          ← Insert process() call + message handlers (MODIFY)
├── effects/enhancement/
│   ├── ColorCorrectionEngine.h    ← Reference for patterns (READ ONLY)
│   └── EdgeMixer.h                ← NEW FILE
├── network/webserver/ws/
│   ├── WsEffectsCommands.cpp      ← Reference for WS handler pattern (READ ONLY)
│   ├── WsDeviceCommands.cpp       ← Add to status (MODIFY)
│   ├── WsEdgeMixerCommands.cpp    ← NEW FILE
│   ├── WsEdgeMixerCommands.h      ← NEW FILE
│   └── WsCommandRouter.cpp        ← Register new handlers (MODIFY)
└── config/features.h              ← DO NOT MODIFY (no feature flag needed; MIRROR mode is the default)
```

## Existing Code Patterns to Follow

### Singleton pattern (from ColorCorrectionEngine):
```cpp
static ColorCorrectionEngine& getInstance() {
    static ColorCorrectionEngine instance;
    return instance;
}
```

### Black pixel skip (from showLeds tone mapping):
```cpp
if ((r | g | b) == 0) continue;  // Skip black pixels
```

### Enum scoping (from Actor.h):
All enums use `enum class` with `uint8_t` backing type.

### WebSocket command registration (from WsCommandRouter):
Follow the `registerHandler("command_name", handlerFunction)` pattern.

## Build & Validation

### Build command:
```bash
cd firmware-v3
pio run -e esp32dev_audio_pipelinecore
```

### Validation criteria (pass/fail):

1. **Build succeeds** with zero warnings related to EdgeMixer code.
2. **MIRROR mode (default):** Behaviour identical to current — no visual change, zero overhead. Verify `process()` returns immediately when mode == MIRROR.
3. **ANALOGOUS mode:** Strip 2 pixels have hue shifted by +spread/2 relative to Strip 1. Black pixels unchanged.
4. **COMPLEMENTARY mode:** Strip 2 pixels have hue shifted by +128. Saturation reduced by ~15%.
5. **Timing:** `process()` completes in <0.5ms for 160 pixels. Can verify with `esp_timer_get_time()` bracketing the call.
6. **No heap allocation:** grep the new file for `new `, `malloc`, `String` — zero hits.
7. **WebSocket commands:** `edge_mixer.set` and `edge_mixer.get` work correctly.
8. **Status includes mixer state:** `edgeMixerMode`, `edgeMixerSpread`, `edgeMixerStrength` appear in status response.

### Timing measurement (add temporarily for validation, remove before merge):
```cpp
// In showLeds(), around the EdgeMixer call:
uint32_t t0 = esp_timer_get_time();
enhancement::EdgeMixer::getInstance().process(m_strip2, LedConfig::LEDS_PER_STRIP);
uint32_t dt = esp_timer_get_time() - t0;
if (dt > 500) { ESP_LOGW("MIXER", "EdgeMixer took %u us", dt); }
```

## What NOT to Do

- Do NOT modify the effect rendering pipeline or EffectContext. The mixer is a post-processor only.
- Do NOT add a feature flag. The MIRROR default mode provides zero-overhead backward compatibility.
- Do NOT use CRGB16 or SQ15x16. We're operating on 8-bit CRGB directly.
- Do NOT allocate scratch buffers. The mixer works in-place on m_strip2.
- Do NOT change the strip1/strip2 memcpy split. The mixer operates AFTER the split.
- Do NOT add any REST API endpoints. WebSocket only (all real-time control is WS).
- Do NOT modify any existing effect code.
- Do NOT use `rgb2hsv_approximate()` alternatives that require `<math.h>` float trig. FastLED's integer approximation is sufficient and fast.

## Summary of Deliverables

| # | File | Action | Lines est. |
|---|------|--------|-----------|
| 1 | `src/effects/enhancement/EdgeMixer.h` | CREATE | ~120 |
| 2 | `src/core/actors/RendererActor.h` | ADD 1 include | 1 |
| 3 | `src/core/actors/RendererActor.cpp` | ADD 1 process() call + 3 message cases | ~15 |
| 4 | `src/core/actors/Actor.h` | ADD 3 MessageType values | 3 |
| 5 | `src/network/webserver/ws/WsEdgeMixerCommands.h` | CREATE | ~30 |
| 6 | `src/network/webserver/ws/WsEdgeMixerCommands.cpp` | CREATE | ~80 |
| 7 | `src/network/webserver/ws/WsCommandRouter.cpp` | REGISTER handlers | ~5 |
| 8 | `src/network/webserver/ws/WsDeviceCommands.cpp` | ADD 3 fields to status | 3 |
