# Phase 1+2: Render Primitives Library + Frame Post-Process

## RBDO Gate

**GROUNDED** — all premises traced to canonical upstream audits (SB 3.1.0/4.1.1, Emotiscope 1.2), PIPELINE_REFORM.md, handoff.md, and verified codebase paths. Captain has approved Option C (full 5-layer reform) with fast-track ordering: Phase 1→2→5, then 3→4→6→7→8. All §8 decisions resolved — see §DECISIONS below.

## Context

LightwaveOS visual pipeline overhaul. The root cause of "spazz" (jittery, unstable visuals) is that every effect rolls its own pixel-level rendering and trail logic, most using `fadeToBlackByDt(K<255)` — a pattern with **no upstream ancestor in SB 3.1.0** that is inherently dt-jitter-sensitive.

This task implements Layer 4 (render primitives) and Layer 5 (frame post-process) of the 5-layer reform. These are **pure additive** — no existing effects are modified. Zero regression risk.

## Task

Create the render primitives library and frame post-process module. Implement, unit-test, and verify compilation. Do NOT modify any existing effect files.

## Decisions (all resolved — do not re-ask)

- **§8.1 Location:** New directory `firmware-v3/src/effects/render/`. Not CoreEffects.h.
- **§8.4 Phase 5 order:** LGPWaveCollision first (not this task, but informs primitive design).
- **§8.8 tempi[] bank:** 8 bins (Phase 3, not this task).
- **§8.9 MOOD-LP:** Producer-side in AudioActor (Phase 3, not this task).
- **§8.10 BeatDetectionAPI:** Thin wrapper (Phase 4, not this task).
- **§8.11 Golden-frame harness:** Reuse `firmware-v3/test/test_golden/` for unit tests.

## Hard Constraints (read back before starting)

- **Centre origin:** LED 79/80 outward (or inward to 79/80). All primitives MUST default to centre-origin addressing. `centerPoint` is available in `EffectContext` (value: 80 for K1v2).
- **No heap alloc in render path:** No `new`/`malloc`/`String` in any function callable from `render()`. Use static buffers. The primitives WILL be called from render() — they must be allocation-free.
- **2.0ms ceiling:** Each primitive must complete well under 2.0ms for 320 LEDs. Target <0.3ms per primitive call.
- **British English:** All comments, docs, variable names where English words appear. Use centre, colour, initialise, behaviour, etc.
- **No rainbows:** No full hue-wheel sweeps in any default colour mapping.
- **CRGB is FastLED native** — `#include <FastLED.h>`, no typedef.
- **Include convention:** Relative paths (`../../plugins/api/EffectContext.h`). See existing effects for pattern.

## Reference Files (READ BEFORE CODING)

Read these in order:

1. `firmware-v3/docs/research/spazz_redesign_2026-04-30/PIPELINE_REFORM.md` — the reform spec. Layer 4 and Layer 5 definitions are your requirements.
2. `firmware-v3/docs/research/spazz_redesign_2026-04-30/canonical_emotiscope_active.md` — upstream `draw_dot`, `draw_sprite`, `apply_frame_blending` implementations. These are your reference implementations.
3. `firmware-v3/docs/research/spazz_redesign_2026-04-30/canonical_SB_4_1_1.md` — SB's bloom sprite scroll and mood-scaled smoothing patterns.
4. `firmware-v3/docs/research/spazz_redesign_2026-04-30/canonical_SB_3_1_0.md` — SB 3.1.0 baseline (NO fadeToBlackBy usage — this confirms the correct trail approach).
5. `firmware-v3/src/plugins/api/EffectContext.h` — the context struct your primitives receive.
6. `firmware-v3/src/effects/enhancement/SmoothingEngine.h` — existing `ExpDecay`, `AsymmetricFollower`, `Spring` (do NOT duplicate; reference where useful).
7. `firmware-v3/src/effects/CoreEffects.h` — existing centre-origin macros (`CENTER_LEFT=79`, `CENTER_RIGHT=80`, `HALF_LENGTH=80`, `STRIP_LENGTH=160`). Reference these constants, do not redefine.
8. `firmware-v3/src/effects/PersistenceHelpers.h` — the existing `fadeToBlackByDt` implementation (this is what you're REPLACING at the global level; understand it but do not call it from primitives).

## Deliverables

### File 1: `firmware-v3/src/effects/render/RenderPrimitives.h`

Header-only where possible (inline functions for performance in render path). Namespace: `lightwaveos::effects::render`.

**Function 1 — `drawDot`**

```cpp
/// Draw a sub-pixel positioned dot with optional motion-blur trail.
/// Position is in normalised space: 0.0 = centre, 1.0 = edge.
/// Sub-pixel interpolated for smooth animation.
///
/// Reference: Emotiscope draw_dot() — per-slot position memory + motion-blur line render.
///
/// @param leds        LED buffer to write to
/// @param ledCount    Number of LEDs in buffer
/// @param centrePoint Centre LED index (80 for K1v2 standard config)
/// @param position    Normalised position (0.0 = centre, 1.0 = edge)
/// @param colour      CRGB colour to draw
/// @param opacity     Brightness scalar (0.0–1.0)
/// @param prevPosition Previous frame's position for motion-blur trail (-1.0 to skip trail)
/// @param mirror      If true, draw symmetrically on both sides of centre (default: true)
inline void drawDot(CRGB* leds, uint16_t ledCount, uint16_t centrePoint,
                    float position, CRGB colour, float opacity,
                    float prevPosition = -1.0f, bool mirror = true);
```

**Function 2 — `drawSpriteScrolled`**

```cpp
/// Additive sub-pixel buffer scroll with multiplicative fade.
/// THE wave-propagation primitive. Scrolls existing buffer content outward
/// from centre by scrollAmount LEDs per frame, fading by alpha each frame.
///
/// Reference: Emotiscope draw_sprite() — additive sub-pixel scroll with alpha fade.
/// Reference: SB bloom — shift-register copy (leds_temp[N-1-i] = leds_last[N-1-i-OFFSET]).
///
/// This is dt-corrected: scrollAmount should be pre-scaled by deltaTimeSeconds.
///
/// @param leds          LED buffer (read + write in-place)
/// @param ledCount      Number of LEDs in buffer
/// @param centrePoint   Centre LED index
/// @param scrollAmount  Fractional LEDs to scroll outward from centre per call (pre-scaled by dt)
/// @param alpha         Multiplicative fade per frame (0.0 = instant clear, 1.0 = no fade).
///                      Typical range: 0.90–0.99. dt-corrected internally.
/// @param dt            Delta time in seconds (for dt-correcting alpha)
inline void drawSpriteScrolled(CRGB* leds, uint16_t ledCount, uint16_t centrePoint,
                                float scrollAmount, float alpha, float dt);
```

**Function 3 — `fillFromBins`**

```cpp
/// Map frequency bins (spectrogram, chromagram, or octave bands) to LEDs,
/// centre-origin, with per-bin colour from palette.
///
/// Bins are mapped symmetrically outward from centrePoint. Bin 0 maps to the
/// centre pair; bin N-1 maps to the edges.
///
/// @param leds        LED buffer to write to
/// @param ledCount    Number of LEDs in buffer
/// @param centrePoint Centre LED index
/// @param bins        Array of bin values (0.0–1.0 normalised)
/// @param binCount    Number of bins
/// @param palette     Colour palette for bin-to-colour mapping
/// @param brightness  Master brightness scalar (0–255)
/// @param additive    If true, add to existing buffer; if false, overwrite (default: false)
inline void fillFromBins(CRGB* leds, uint16_t ledCount, uint16_t centrePoint,
                          const float* bins, uint8_t binCount,
                          const CRGBPalette16& palette, uint8_t brightness,
                          bool additive = false);
```

### File 2: `firmware-v3/src/effects/render/FrameBlend.h`

```cpp
/// Whole-image one-pole IIR frame blending, controlled by mood parameter.
/// Replaces per-effect fadeToBlackByDt calls with a single global post-process.
///
/// Reference: Emotiscope apply_frame_blending() — one-pole IIR after all effects.
///
/// mood=0   → blendCoeff=0.0  (no blend, snappy, full overwrite each frame)
/// mood=255 → blendCoeff=0.92 (soft trails, heavy persistence)
///
/// Must be called ONCE per frame AFTER all effects have rendered, BEFORE FastLED.show().
///
/// @param leds           Current frame LED buffer (modified in-place)
/// @param prevFrame      Previous frame buffer (static, caller-owned, updated by this function)
/// @param ledCount       Number of LEDs
/// @param mood           Mood parameter (0–255)
/// @param dt             Delta time in seconds (for dt-correcting blend coefficient)
void applyFrameBlending(CRGB* leds, CRGB* prevFrame, uint16_t ledCount,
                         uint8_t mood, float dt);
```

**Implementation notes for `applyFrameBlending`:**
- The `prevFrame` buffer is static and owned by the caller (RendererActor). It must be allocated once at init, NOT per-frame. 320 × 3 bytes = 960 bytes in DRAM — acceptable.
- The blend coefficient derivation: `float blendCoeff = (mood / 255.0f) * 0.92f;`
- dt-correction: `float dtCorrected = powf(blendCoeff, dt * 120.0f);` (normalised to 120 FPS reference)
- Per-LED: `leds[i] = blend(prevFrame[i], leds[i], (1.0f - dtCorrected) * 255);` then `prevFrame[i] = leds[i];`
- FastLED's `blend()` function handles the CRGB interpolation.

### File 3: `firmware-v3/src/effects/render/RenderPrimitives.cpp`

Only needed if any function bodies are too large for inline (>20 lines). Otherwise, keep everything in the header. Use your judgement — but remember: these are called at 120 FPS from render(). Inline is strongly preferred for <0.3ms budget.

### File 4: Unit Tests

Create `firmware-v3/test/test_render_primitives/test_render_primitives.cpp` using the existing test infrastructure pattern from `firmware-v3/test/test_golden/`.

**Test cases (minimum):**

1. **`drawDot` centre placement:** Call with position=0.0, verify LED 79 and 80 are lit (centre pair), all others black.
2. **`drawDot` edge placement:** Call with position=1.0, verify LEDs 0 and 159 are lit, centre is black.
3. **`drawDot` sub-pixel interpolation:** Call with position=0.5, verify brightness is distributed across adjacent LEDs (not a single hard pixel).
4. **`drawDot` motion trail:** Call with prevPosition=0.2, position=0.4, verify LEDs between the two positions have non-zero brightness (trail fill).
5. **`drawDot` mirror=false:** Call with mirror=false, verify only one side of centre is lit.
6. **`drawSpriteScrolled` outward scroll:** Seed centre pair with white, call with scrollAmount=1.0, verify centre pair has moved outward by 1 LED (with sub-pixel).
7. **`drawSpriteScrolled` alpha decay:** Call repeatedly with alpha=0.9, verify brightness decreases each frame.
8. **`drawSpriteScrolled` dt-correction:** Call with same parameters but different dt values (8.33ms vs 16.67ms), verify visual result is frame-rate independent (within tolerance).
9. **`fillFromBins` centre-origin mapping:** Call with 8 bins, verify bin[0] maps to centre, bin[7] maps to edges.
10. **`fillFromBins` additive mode:** Pre-fill buffer with red, call fillFromBins with additive=true and blue bins, verify result is purple (additive blend).
11. **`applyFrameBlending` mood=0:** Verify output equals current frame exactly (no blending).
12. **`applyFrameBlending` mood=255:** Verify output heavily favours previous frame (soft trail).
13. **`applyFrameBlending` dt-independence:** Same as drawSpriteScrolled test 8 — different dt, same visual result.
14. **No heap allocation verification:** Run all primitives through a test that hooks malloc/new and asserts zero allocations during the call.

### File 5: PlatformIO test configuration

Add a test environment or verify the existing `native` env can build and run the test. The test must compile and pass on native (host machine), not on-device. Check `platformio.ini` for existing native test patterns.

## What NOT To Do

1. **Do NOT modify any existing effect file.** This is pure additive.
2. **Do NOT modify RendererActor** to call `applyFrameBlending` yet — that's Phase 5 integration work.
3. **Do NOT create base classes** (AmplitudeEffect, ScrollEffect, etc.) — that's Phase 4.
4. **Do NOT touch ControlBus.h** or any audio code — that's Phase 3.
5. **Do NOT use `fadeToBlackByDt` inside any primitive.** The whole point is to replace that pattern.
6. **Do NOT use bare `*= 0.95` smoothing** — always dt-correct using the canonical formula: `alpha = 1.0 - exp(-lambda * dt)` or `powf(base, dt * referenceRate)`.
7. **Do NOT use spatial-aliasing clamp on phase increment at 2.5 rad** — failed approach, made spazz worse.
8. **Do NOT stack AsymmetricFollower + rolling-average + ExpDecay** — doctrinal anti-pattern ("5L-AR").
9. **Do NOT use per-effect Spring on raw audio energy with triple-multiplication on phase rate** — no upstream ancestry.
10. **Do NOT use binary `is*Hit()` triggers driving continuous parameters** — causes 1-frame whole-strip step changes.

## Compilation Verification

After implementation, run:
```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz
```

This must succeed with zero errors. Warnings are acceptable in this phase but should be noted.

For unit tests:
```bash
cd firmware-v3
pio test -e native --filter test_render_primitives
```

If no `native` env exists, check for `test_native` or similar. If none, create the minimum viable test env entry in `platformio.ini`.

## Confidence Gate

Before committing, verify:
1. All 14 test cases pass on native.
2. `pio run -e esp32dev_audio_esv11_k1v2_32khz` succeeds (the new files are included but not yet called from production code — verify no link errors from unused inline functions).
3. No `new`/`malloc`/`String`/heap allocation in any primitive function.
4. All comments and docs use British English (centre, colour, behaviour, initialise).
5. `drawDot` and `drawSpriteScrolled` both respect centrePoint parameter (no hardcoded 79/80).
6. dt-correction is applied in `drawSpriteScrolled` (scroll amount) and `applyFrameBlending` (blend coefficient).

## Return Format

When complete, report:
```
PHASE 1+2 IMPLEMENTATION COMPLETE
Files created: [list with paths]
Files modified: [list — should be minimal, possibly just platformio.ini for test env]
Test results: [pass/fail count]
Compilation: [success/failure + any warnings]
Open issues: [any]
Confidence: [high/medium/low + reasoning]
```
