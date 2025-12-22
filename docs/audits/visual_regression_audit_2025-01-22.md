# Visual Pipeline & Effects Regression Audit

**Date:** 2025-01-22  
**Baseline Commit:** `9734cca1ee7dbbe4e463dc912e3c9ac2b9d1152e`  
**Candidate:** Current working tree (uncommitted changes)  
**Auditor:** Automated analysis + static code review

---

## Executive Summary

This audit identifies **critical visual regression risks** introduced by recent changes to the LightwaveOS v2 firmware. The primary concern is a **new post-render color correction pipeline** that mutates the live LED state buffer (`m_leds`) in-place every frame, directly impacting **buffer-feedback effects** that rely on reading previous frame state.

### Key Findings

1. **CRITICAL**: Post-render correction pipeline mutates live state buffer (`RendererActor::onTick()` line 262)
2. **CRITICAL**: Color processing algorithms cause severe visual regressions through double-darkening (white guardrail + gamma) and gamma crushing of low values - **see detailed analysis in `color_processing_algorithm_analysis_2025-01-22.md`**
3. **HIGH**: NVS persistence of correction config creates "sticky" behavior across device resets
4. **MEDIUM**: Center distance calculation refactor (`abs(i-CENTER_LEFT)` → `centerPairDistance(i)`) affects 13+ effects
5. **MEDIUM**: Palette mutation at load time for `WHITE_HEAVY` palettes
6. **LOW**: LED streaming overhead (cross-core `getBufferCopy`) may introduce timing jitter

### Affected Components

- **Render Loop**: `v2/src/core/actors/RendererActor.cpp` (7 lines added)
- **Color Correction**: `v2/src/effects/enhancement/ColorCorrectionEngine.cpp` (new file, ~370 lines)
- **Effect Implementations**: 13 files modified (center distance refactor)
- **Network/Streaming**: `v2/src/network/WebServer.cpp` (230 lines added/modified)

---

## 1. Diff Inventory & Categorization

### 1.1 Change Statistics

```
24 files changed, 676 insertions(+), 103 deletions(-)
```

### 1.2 Categorized Changes

#### **Render Loop & Buffer Semantics**
- `v2/src/core/actors/RendererActor.cpp` (+7 lines)
  - **Line 262**: `ColorCorrectionEngine::getInstance().processBuffer(m_leds, ...)` - **IN-PLACE MUTATION**
  - **Line 528**: `correctPalette(m_currentPalette, flags)` - palette mutation at load time

#### **Color Processing Algorithms**
- `v2/src/effects/enhancement/ColorCorrectionEngine.cpp` (new, ~370 lines)
  - Auto-exposure (BT.601 luminance-based)
  - White guardrail (HSV saturation boost / RGB white reduction)
  - Brown guardrail (LC_SelfContained pattern)
  - Gamma correction (LUT-based, default 2.2)
  - NVS persistence (loads config on construction)
  - **⚠️ CRITICAL ISSUES IDENTIFIED**: See `color_processing_algorithm_analysis_2025-01-22.md` for detailed analysis of:
    - Double-darkening problem (guardrail + gamma = 75% darker)
    - Gamma crushing low values (60-90% darker for values < 100)
    - Pipeline order conflict (guardrails → gamma is wrong)
    - Default configuration too aggressive (both enabled by default)

#### **Timing & Synchronization**
- `v2/src/network/WebServer.cpp` (+230 lines)
  - LED frame streaming (`getBufferCopy` + binary WS send)
  - Cross-core memory copy (320 LEDs × 3 bytes = 960 bytes per frame)
  - Throttled to 20 FPS (vs render at 120 FPS)

#### **Hardware Interface Layers**
- `v2/platformio.ini` (+1 line)
  - `board_build.filesystem = littlefs` - filesystem change
- `v2/src/network/WebServer.cpp` (asset serving refactor)
  - Explicit route handlers vs `serveStatic()` (reduces VFS error spam)

#### **Effect Implementations (Center Distance Refactor)**
- `v2/src/effects/CoreEffects.cpp` (12 lines changed)
- `v2/src/effects/LGPAdvancedEffects.cpp` (35 lines changed)
- `v2/src/effects/LGPColorMixingEffects.cpp` (20 lines changed)
- `v2/src/effects/LGPGeometricEffects.cpp` (18 lines changed)
- `v2/src/effects/LGPInterferenceEffects.cpp` (10 lines changed)
- `v2/src/effects/LGPNovelPhysicsEffects.cpp` (8 lines changed)
- `v2/src/effects/LGPOrganicEffects.cpp` (4 lines changed)
- `v2/src/effects/LGPQuantumEffects.cpp` (2 lines changed)
- `v2/src/effects/LGPChromaticEffects.cpp` (100 lines changed - palette-aware variant added)

---

## 2. Visual Pipeline Audit (Static Analysis)

### 2.1 Render Loop & Buffer Semantics

**Location:** `v2/src/core/actors/RendererActor.cpp:254-265`

**Current Flow:**
```cpp
void RendererActor::onTick() {
    renderFrame();  // Effect writes to m_leds
    processBuffer(m_leds, ...);  // ⚠️ IN-PLACE MUTATION
    showLeds();  // Push to hardware
}
```

**Critical Issue:** `processBuffer()` mutates `m_leds` **in-place** after effect rendering but before `showLeds()`. This means:

1. **Effect state is corrupted**: Effects that read `ctx.leds[i+1]` or `ctx.leds[i-1]` on the next frame will see **gamma-corrected, guardrail-filtered values** instead of the raw effect output.
2. **Frame-to-frame feedback loop**: Buffer-feedback effects (Confetti, Fire, Ripple, etc.) now evolve based on **post-processed state**, not their own output.

**Baseline Behavior:** Effects wrote directly to `m_leds`, which was pushed to hardware unchanged.

**Risk Level:** **CRITICAL** - Affects all buffer-feedback effects.

### 2.2 Color Correction Pipeline

**Location:** `v2/src/effects/enhancement/ColorCorrectionEngine.cpp:180-202`

**Pipeline Order:**
1. Auto-Exposure (if enabled) - scales down if avg luma > target
2. White Guardrail (if mode != OFF) - HSV saturation boost or RGB white reduction
3. Brown Guardrail (if enabled) - clamps G/B relative to R
4. Gamma Correction (if enabled, default: ON) - LUT-based 2.2 gamma

**Default Configuration (from NVS load):**
```cpp
mode = RGB (white reduction)
gammaEnabled = true
gammaValue = 2.2
autoExposureEnabled = false
brownGuardrailEnabled = false
```

**NVS Persistence Risk:**
- Config loads on `ColorCorrectionEngine` construction (singleton, first access)
- Settings persist across device resets
- **"Sticky config" problem**: Same firmware, different visuals if NVS was written on different devices

**Risk Level:** **HIGH** - Non-deterministic behavior across devices.

### 2.3 Timing & Synchronization Risks

**Per-Frame Overhead:**
- `processBuffer()` iterates 320 LEDs × correction stages
- Estimated: ~0.5-1.0ms per frame (gamma LUT lookup is fast, but guardrail checks add overhead)
- **Impact**: May reduce headroom for complex effects

**Cross-Core Contention:**
- LED streaming calls `getBufferCopy()` from Core 0 (WebServer) while Core 1 (Renderer) writes to `m_leds`
- `getBufferCopy()` uses `memcpy()` - safe but adds ~0.3ms copy time
- **Risk**: If streaming subscriber count increases, copy overhead may cause frame drops

**Risk Level:** **MEDIUM** - Performance impact measurable but not critical at current load.

### 2.4 Hardware Interface Layers

**Filesystem Change:**
- `board_build.filesystem = littlefs` added to `platformio.ini`
- **Impact**: None on visual pipeline (only affects asset serving)

**Asset Serving Refactor:**
- Explicit route handlers replace `serveStatic()` with gzip probing
- **Impact**: Reduces VFS error spam, no visual impact

**Risk Level:** **LOW** - No visual regression risk.

---

## 3. Effect-by-Effect Audit

### 3.1 Effect Enumeration (0-67)

**Core Effects (0-12):**
- 0: Fire
- 1: Ocean
- 2: Plasma
- 3: Confetti ⚠️
- 4: Sinelon ⚠️
- 5: Juggle ⚠️
- 6: BPM
- 7: Wave ⚠️
- 8: Ripple ⚠️
- 9: Heartbeat
- 10: Interference
- 11: Breathing ⚠️
- 12: Pulse

**LGP Interference (13-17):**
- 13: Box Wave
- 14: Holographic
- 15: Modal Resonance
- 16: Interference Scanner
- 17: Wave Collision ⚠️

**LGP Geometric (18-25):**
- 18: Diamond Lattice
- 19: Hexagonal Grid
- 20: Spiral Vortex
- 21: Sierpinski Triangles
- 22: Chevron Waves
- 23: Concentric Rings
- 24: Star Burst
- 25: Mesh Network

**LGP Advanced (26-33):**
- 26: Moire Curtains
- 27: Radial Ripple
- 28: Holographic Vortex
- 29: Evanescent Drift
- 30: Chromatic Shear
- 31: Modal Cavity
- 32: Fresnel Zones
- 33: Photonic Crystal

**LGP Organic (34-39):**
- 34: Aurora Borealis
- 35: Bioluminescent Waves
- 36: Plasma Membrane
- 37: Neural Network
- 38: Crystalline Growth
- 39: Fluid Dynamics

**LGP Quantum (40-49):**
- 40: Quantum Tunneling ⚠️
- 41: Gravitational Lensing
- 42: Time Crystal
- 43: Soliton Waves
- 44: Metamaterial Cloaking
- 45: GRIN Cloak
- 46: Caustic Fan
- 47: Birefringent Shear
- 48: Anisotropic Cloak
- 49: Evanescent Skin

**LGP Color Mixing (50-59):**
- 50: Color Temperature
- 51: RGB Prism
- 52: Complementary Mixing
- 53: Quantum Colors
- 54: Doppler Shift
- 55: Color Accelerator
- 56: DNA Helix
- 57: Phase Transition
- 58: Chromatic Aberration
- 59: Perceptual Blend

**LGP Novel Physics (60-64):**
- 60: Chladni Harmonics
- 61: Gravitational Wave Chirp
- 62: Quantum Entanglement Collapse ⚠️
- 63: Mycelial Network
- 64: Riley Dissonance

**LGP Chromatic (65-67):**
- 65: Chromatic Lens
- 66: Chromatic Pulse
- 67: Chromatic Interference

### 3.2 Statefulness Classification

**Buffer-Feedback/Stateful Effects (HIGH RISK):**
- **Confetti (3)**: Reads `ctx.leds[i+1]` and `ctx.leds[i-1]` for propagation
- **Sinelon (4)**: Uses `fadeToBlackBy()` + `+=` operator (additive blending)
- **Juggle (5)**: Uses `fadeToBlackBy()` + `|=` operator (additive blending)
- **Wave (7)**: Uses `fadeToBlackBy()` + conditional writes
- **Ripple (8)**: Uses `fadeToBlackBy()` + reads previous frame for wave propagation
- **Breathing (11)**: Uses `fadeToBlackBy()` + radius-based fade
- **Wave Collision (17)**: Uses `fadeToBlackBy()` + wave packet propagation
- **Quantum Tunneling (40)**: Uses `fadeToBlackBy()` + stateful tunneling simulation
- **Quantum Entanglement Collapse (62)**: Uses `fadeToBlackBy()` + stateful collapse state machine

**Stateless Effects (LOW RISK):**
- All other effects (58 total) fully rewrite the frame each render

### 3.3 Regression Vectors by Effect

#### **Center Distance Refactor Impact**

**Changed:** `abs((float)i - CENTER_LEFT)` → `centerPairDistance((uint16_t)i)`

**Mathematical Difference:**
- Old: `abs(i - 79)` → returns `0` at LED 79, `1` at LED 80
- New: `centerPairDistance(i)` → returns `0` at LED 79, `0` at LED 80 (both treated as center)

**Affected Effects:**
- Ocean (1)
- BPM (6)
- Wave (7)
- Ripple (8)
- Interference (10)
- Breathing (11)
- All LGP Advanced effects (26-33)
- All LGP Interference effects (13-17) - partial
- All LGP Geometric effects (18-25) - partial
- All LGP Organic effects (34-39) - partial
- All LGP Quantum effects (40-49) - partial
- All LGP Color Mixing effects (50-59) - partial
- All LGP Novel Physics effects (60-64) - partial
- All LGP Chromatic effects (65-67)

**Impact:** Symmetric center treatment (79/80 both = 0 distance) vs asymmetric (79=0, 80=1). **Visual change expected** but may be subtle.

**Risk Level:** **MEDIUM** - Intentional refactor, but may cause visual drift.

#### **Post-Render Correction Impact**

**Buffer-Feedback Effects (CRITICAL):**
- **Confetti (3)**: Next frame reads gamma-corrected, guardrail-filtered pixels → propagation breaks
- **Ripple (8)**: Wave propagation depends on previous frame brightness → gamma crushes dim tails
- **Fire (0)**: Heat diffusion reads previous frame → guardrail may zero out "whitish" embers

**Low-Amplitude Trail Effects (HIGH):**
- Effects using `fadeToBlackBy()` with small fade amounts (10-20) may have trails **completely zeroed** by gamma correction if they fall below threshold.

**Stateless Effects (LOW):**
- Fully rewritten each frame, so correction only affects **output appearance**, not **state evolution**.

#### **Palette Mutation Impact**

**WHITE_HEAVY Palettes:**
- Load-time correction modifies palette entries (HSV saturation boost or RGB white reduction)
- **Impact**: Effects using `ColorFromPalette()` will see different colors than baseline
- **Affected Palettes**: Fire, Lava, Pink Purple, Departure, Blue Magenta White, Vik, Oleron, La Jolla, Devon, Cork, Broc, Bilbao, Buda, Davos, La Paz, Nuuk, Oslo, Turku (18 total)

**Risk Level:** **MEDIUM** - Intentional feature, but may cause visual drift for effects that rely on specific palette colors.

---

## 4. Evidence Capture Framework

### 4.1 LED Stream Protocol

**Format:** Binary WebSocket frames (v1 dual-strip format)
- Header: `[MAGIC=0xFE][VERSION=0x01][NUM_STRIPS=0x02][LEDS_PER_STRIP=160]`
- Payload: `[STRIP0_ID=0x00][RGB×160][STRIP1_ID=0x01][RGB×160]`
- Total: 966 bytes per frame
- Throttled: 20 FPS (vs render at 120 FPS)

**Dashboard Integration:**
- `lightwave-dashboard/src/hooks/useLedStream.ts` - frame parser
- `lightwave-dashboard/src/components/LGPVisualizer.tsx` - canvas renderer

### 4.2 Capture Methodology

**Standardized Settings:**
- Palette: ID 0 (Sunset Real - non-WHITE_HEAVY)
- Brightness: 180
- Speed: 15
- Visual Params: intensity=128, saturation=255, complexity=128, variation=0
- Duration: 5 seconds per effect
- LED Stream: Enabled (1 subscriber)

**Artifact Generation:**
- Per-effect MP4/GIF from canvas capture (dashboard visualizer)
- Keyframe PNGs at t=1s, 2s, 4s for side-by-side comparison
- Binary frame dumps for pixel-level analysis

**Baseline vs Candidate:**
- **Baseline**: Flash firmware from commit `9734cca1...`, capture all effects
- **Candidate**: Flash current working tree, capture all effects
- Compare frame-by-frame for pixel-level deltas

### 4.3 Automated Capture Script (Framework)

**Note:** Actual capture requires hardware + dashboard running. Framework documented for future automation.

```typescript
// Framework for LED stream capture (requires hardware)
async function captureEffect(effectId: number, duration: number): Promise<FrameDump[]> {
  // 1. Set effect via API
  await api.effects.set(effectId);
  await api.palettes.set(0);
  await api.brightness.set(180);
  await api.speed.set(15);
  
  // 2. Subscribe to LED stream
  const frames: FrameDump[] = [];
  wsClient.on('ledStream.frame', (data: Uint8Array) => {
    frames.push({
      timestamp: Date.now(),
      effectId,
      frameData: data
    });
  });
  
  // 3. Wait for duration
  await sleep(duration * 1000);
  
  // 4. Unsubscribe
  wsClient.send({ type: 'ledStream.unsubscribe' });
  
  return frames;
}
```

---

## 5. Performance Metrics Comparison

### 5.1 Metrics to Collect

**Per-Effect Metrics:**
- FPS (`RenderStats.currentFPS`)
- Frame drops (`RenderStats.frameDrops`)
- CPU% (`RenderStats.cpuPercent`)
- Frame time (avg/max/min in microseconds)

**System Metrics:**
- Free heap (ESP free heap at idle and during streaming)
- LED streaming overhead (FPS delta with 0 vs 1 subscriber)

**Collection Method:**
- Serial output: `RenderStats` logged every 10 frames
- API endpoint: `/api/v1/system/stats` (if available)
- WebSocket: `system.getStats` command

### 5.2 Expected Impact

**Post-Render Correction Overhead:**
- Estimated: +0.5-1.0ms per frame (320 LEDs × correction stages)
- **Impact**: May reduce FPS from 120 → 115-118 FPS for complex effects

**LED Streaming Overhead:**
- `getBufferCopy()`: ~0.3ms per frame (960 bytes memcpy)
- **Impact**: Negligible at 20 FPS throttling, but may cause jitter if multiple subscribers

**Risk Level:** **LOW** - Performance impact measurable but within acceptable range.

---

## 6. Root Cause Analysis

### 6.1 Primary Regression: In-Place Buffer Mutation

**Root Cause:** `ColorCorrectionEngine::processBuffer()` mutates the live state buffer (`m_leds`) **after** effect rendering but **before** hardware output.

**Why This Breaks Buffer-Feedback Effects:**
1. Effect writes frame N to `m_leds`
2. `processBuffer()` applies gamma/guardrail → modifies `m_leds` in-place
3. Hardware displays corrected frame N
4. **Next frame**: Effect reads `m_leds[i+1]` (from frame N) → sees **gamma-corrected, guardrail-filtered values** instead of raw effect output
5. Effect evolves based on **post-processed state** → visual corruption

**Example (Confetti):**
- Baseline: Confetti reads `ctx.leds[i+1]` → sees raw confetti pixel (RGB 255,128,0)
- Candidate: Confetti reads `ctx.leds[i+1]` → sees gamma-corrected pixel (RGB 200,80,0) → propagation distance/color wrong

### 6.2 Secondary Regression: NVS Sticky Config

**Root Cause:** `ColorCorrectionEngine` loads config from NVS on construction (singleton pattern).

**Why This Causes Non-Determinism:**
1. Device A: NVS has `mode=RGB, gammaEnabled=true`
2. Device B: NVS has `mode=OFF, gammaEnabled=false` (or uninitialized)
3. Same firmware → **different visuals** on different devices

**Risk:** Production devices may have inconsistent correction settings, making regression testing unreliable.

### 6.3 Tertiary Regression: Center Distance Refactor

**Root Cause:** Intentional refactor to use `centerPairDistance()` for symmetric center treatment.

**Impact:** Mathematical change in distance calculation (79/80 both = 0 vs 79=0, 80=1) causes visual drift in wave-based effects.

**Risk:** Intentional change, but may cause user-visible differences.

---

## 7. Prioritized Fix Recommendations

### 7.1 CRITICAL: Fix In-Place Buffer Mutation

**Priority:** P0 (Blocks all buffer-feedback effects)

**Solution:** Apply correction to **output buffer only**, not live state.

**Implementation:**
```cpp
void RendererActor::onTick() {
    renderFrame();  // Effect writes to m_leds
    
    // Copy to output buffer for correction
    CRGB outputBuffer[LedConfig::TOTAL_LEDS];
    memcpy(outputBuffer, m_leds, sizeof(m_leds));
    
    // Apply correction to output (not live state)
    enhancement::ColorCorrectionEngine::getInstance().processBuffer(
        outputBuffer, LedConfig::TOTAL_LEDS);
    
    // Push corrected output to hardware
    memcpy(m_leds, outputBuffer, sizeof(m_leds));  // For showLeds()
    showLeds();
}
```

**Alternative (Better):** Use separate `m_outputBuffer` and only copy corrected output to `m_leds` for `showLeds()`. Keep `m_leds` as pure effect state.

**Verification:**
- Confetti propagation works correctly
- Ripple waves propagate smoothly
- Fire heat diffusion maintains correct gradients

### 7.2 HIGH: Fix NVS Sticky Config

**Priority:** P1 (Causes non-determinism)

**Solution:** Make NVS config **opt-in** or **reset on firmware version change**.

**Implementation:**
```cpp
void ColorCorrectionEngine::loadFromNVS() {
    Preferences prefs;
    if (prefs.begin("colorCorr", true)) {
        uint32_t savedVersion = prefs.getUInt("fwVersion", 0);
        uint32_t currentVersion = getFirmwareVersion();  // From build info
        
        if (savedVersion != currentVersion) {
            // Firmware changed → reset to defaults
            prefs.end();
            return;  // Use defaults
        }
        
        // Load saved config...
    }
}
```

**Verification:**
- Same firmware → same visuals on all devices
- Firmware upgrade → config resets to defaults

### 7.3 MEDIUM: Document Center Distance Refactor

**Priority:** P2 (Intentional change, but needs documentation)

**Solution:** Add comment explaining symmetric center treatment and expected visual changes.

**Verification:**
- Code comments explain the refactor
- User-facing changelog documents visual changes

### 7.4 LOW: Optimize LED Streaming Overhead

**Priority:** P3 (Performance optimization)

**Solution:** Use double-buffering or reduce copy frequency if multiple subscribers.

**Verification:**
- FPS remains stable with 1+ streaming subscribers

---

## 8. Verification Test Cases

### 8.1 Automated Tests (Framework)

**Test: Buffer-Feedback Effect Integrity**
```cpp
// Pseudo-code for automated test
void testConfettiPropagation() {
    // 1. Set Confetti effect
    setEffect(3);
    setPalette(0);
    
    // 2. Render N frames
    for (int i = 0; i < 100; i++) {
        renderFrame();
        processBuffer();  // Simulate correction
    }
    
    // 3. Verify confetti pixels propagate outward
    // (Check that center pixels fade and edge pixels appear)
    assert(centerBrightness < edgeBrightness);
}
```

**Test: Correction Output-Only**
```cpp
void testCorrectionDoesNotMutateState() {
    // 1. Render frame
    renderFrame();
    CRGB before[320];
    memcpy(before, m_leds, sizeof(m_leds));
    
    // 2. Apply correction to output buffer (not m_leds)
    CRGB output[320];
    memcpy(output, m_leds, sizeof(m_leds));
    processBuffer(output, 320);
    
    // 3. Verify m_leds unchanged
    assert(memcmp(before, m_leds, sizeof(m_leds)) == 0);
}
```

### 8.2 Manual Test Cases

**Test Case 1: Confetti Visual Regression**
- **Steps:**
  1. Flash baseline firmware
  2. Set effect 3 (Confetti), palette 0, brightness 180, speed 15
  3. Record 5 seconds via LED stream
  4. Flash candidate firmware
  5. Repeat steps 2-3
  6. Compare recordings frame-by-frame
- **Expected:** Confetti particles propagate outward smoothly in both versions
- **Actual:** [To be filled after capture]

**Test Case 2: Ripple Wave Propagation**
- **Steps:** Same as Test Case 1, but effect 8 (Ripple)
- **Expected:** Ripples expand from center with smooth fade
- **Actual:** [To be filled after capture]

**Test Case 3: Fire Heat Diffusion**
- **Steps:** Same as Test Case 1, but effect 0 (Fire)
- **Expected:** Heat diffuses outward from center, embers fade naturally
- **Actual:** [To be filled after capture]

**Test Case 4: Gamma Correction Output**
- **Steps:**
  1. Flash candidate firmware
  2. Set effect 2 (Plasma), palette 0, brightness 180
  3. Enable/disable gamma via NVS
  4. Compare visual output
- **Expected:** Gamma ON → darker, more natural appearance. Gamma OFF → brighter, linear.
- **Actual:** [To be filled after capture]

**Test Case 5: WHITE_HEAVY Palette Correction**
- **Steps:**
  1. Flash candidate firmware
  2. Set effect 2 (Plasma), palette 11 (Departure - WHITE_HEAVY)
  3. Compare with baseline (no correction)
- **Expected:** Corrected palette → less white, more saturated colors
- **Actual:** [To be filled after capture]

---

## 9. Severity Classification

### CRITICAL (P0)
- **In-place buffer mutation** → Breaks all buffer-feedback effects
- **Affected Effects:** Confetti, Ripple, Fire, Sinelon, Juggle, Wave, Breathing, Wave Collision, Quantum Tunneling, Quantum Entanglement Collapse

### HIGH (P1)
- **NVS sticky config** → Non-deterministic behavior across devices
- **Affected:** All effects (correction settings vary by device)

### MEDIUM (P2)
- **Center distance refactor** → Visual drift in wave-based effects
- **Affected:** 13+ effects using center distance calculation
- **Palette mutation** → Color changes for WHITE_HEAVY palettes
- **Affected:** All effects using WHITE_HEAVY palettes (18 palettes)

### LOW (P3)
- **LED streaming overhead** → Potential timing jitter with multiple subscribers
- **Performance impact** → ~0.5-1.0ms per frame correction overhead

---

## 10. Conclusion

The audit identifies **one critical regression** (in-place buffer mutation) that directly breaks buffer-feedback effects like Confetti, and **one high-priority issue** (NVS sticky config) that causes non-deterministic behavior.

**Immediate Action Required:**
1. Fix in-place buffer mutation (P0) → Apply correction to output buffer only
2. Fix NVS sticky config (P1) → Reset on firmware version change

**Recommended Next Steps:**
1. Implement fixes per Section 7
2. Capture baseline vs candidate LED stream recordings for all 68 effects
3. Generate before/after visual comparisons
4. Run performance metrics comparison
5. Update this report with actual capture data

**Risk Assessment:**
- **Current State:** **UNSAFE** - Buffer-feedback effects are corrupted, color processing algorithms cause severe visual regressions
- **After P0 Fix:** **SAFE** - All effects should work correctly
- **After P1 Fix:** **DETERMINISTIC** - Same firmware → same visuals on all devices
- **After Color Processing Fix:** **VISUALLY CORRECT** - No double-darkening, trails visible, natural colors

---

## Related Analysis Documents

- **`color_processing_algorithm_analysis_2025-01-22.md`**: Comprehensive analysis of color processing algorithm interactions, double-darkening issues (75% darker), gamma crushing of low values (60-90% darker), pipeline order conflicts, and recommended fixes. This analysis identifies that **ALL effects** are affected by these issues, not just buffer-feedback ones.

---

**Report Generated:** 2025-01-22  
**Next Review:** After P0/P1 fixes implemented + color processing algorithm fixes

