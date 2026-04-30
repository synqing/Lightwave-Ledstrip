# Phase 5 Proof-of-Concept: K1 Bloom V1 (0x1301) Canonical SB Rewrite

## RBDO Gate

**GROUNDED** — premises traced to: verbatim canonical SB 4.1.1 `light_mode_bloom()` (lightshow_modes.h:398-499, documented in canonical_SB_4_1_1.md §2.4), canonical `draw_sprite()` (led_utilities.h:1247-1290, documented §3.6), 9-divergence table from full source comparison of SbK1BloomEffect.cpp vs canonical SB, Phase 1+2 `drawSpriteScrolled` implementation (committed 4a22af6f, RenderPrimitives.cpp:147-192), CRGB_F type definition (SbK1BaseEffect.h:43-70), branch state verified at ac413d33.

## Context

K1 Bloom V1 (0x1301) is "almost static" — centre-origin trail barely moves. The CC agent's DRAWING BOARD analysis compared every line of SbK1BloomEffect.cpp against canonical SB 4.1.1 `light_mode_bloom()` and identified 9 divergences. The DOMINANT cause: V1 uses integer scroll (lines 176-184) where canonical SB uses sub-pixel additive scroll via `draw_sprite()`. When V1's `pixelsToScroll` truncates to 0 (~26% of frames at default mood), the trail wipes entirely — no persistence.

**This task rewrites V1's `renderEffect()` to use `drawSpriteScrolled` from the Phase 1+2 render primitives library.** This makes 0x1301 the FIRST effect to integrate the new primitives — a Phase 5 proof-of-concept that validates the 5-layer architecture.

## Hard Constraints

- esptool-direct flash only (NOT `pio run -t upload`)
- Read K1v2 flash procedure from memory files before flashing
- Verify MAC (K1v2: `b4:3a:45:a5:87:f8`) before flash
- Centre origin (LED 79/80 outward), no heap in render(), 2.0ms ceiling, British English
- Do NOT modify any file other than `SbK1BloomEffect.cpp` and `SbK1BloomEffect.h`
- Do NOT modify the base class (`SbK1BaseEffect`)
- Do NOT modify RenderPrimitives.h/.cpp or FrameBlend.h/.cpp
- Do NOT modify PersistenceHelpers.h

## Reference Files (READ BEFORE CODING)

Read these in this exact order:

1. `firmware-v3/docs/research/spazz_redesign_2026-04-30/canonical_SB_4_1_1.md` §2.4 — verbatim canonical `light_mode_bloom()` source and per-frame motion equation. **This is your ground truth.**
2. `firmware-v3/docs/research/spazz_redesign_2026-04-30/canonical_SB_4_1_1.md` §3.6 — verbatim `draw_sprite()` source. **This is what `drawSpriteScrolled` implements.**
3. `firmware-v3/src/effects/render/RenderPrimitives.h` — the `drawSpriteScrolled` signature and design contract.
4. `firmware-v3/src/effects/render/RenderPrimitives.cpp` lines 147-192 — `drawSpriteScrolled` implementation. Understand: it operates on `CRGB*` (uint8), not `CRGB_F*` (float). It snapshots→clears→scatters with sub-pixel interpolation and dt-corrected alpha.
5. `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BloomEffect.cpp` — the CURRENT broken implementation you're rewriting.
6. `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BloomEffect.h` — header with PSRAM struct, constants, private methods.
7. `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BaseEffect.h` — CRGB_F type, base class API, `m_dt`, `m_chromaSmooth[]`, `m_huePosition`, `m_noveltyCurve[]`, `paletteColorF()`, `applyContrast()`.
8. `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BaseEffect.cpp` — `baseProcessAudio()` which sets `m_dt` every frame.

## The 9 Divergences (from DRAWING BOARD analysis)

For reference — these are the specific differences between V1's current code and canonical SB:

| # | Feature | Canonical SB 4.1.1 | K1 V1 Current | Impact |
|---|---------|---------------------|----------------|--------|
| 1 | **Scroll mechanism** | Sub-pixel additive via `draw_sprite(position=0.25+1.75*MOOD, alpha=0.99)` | Integer truncation: `pixelsToScroll = (int)m_scrollAccum`; when 0, trail wipes | **DOMINANT** — causes "almost static" |
| 2 | Colour synthesis | `bin*bin * (1/6)` per chroma bin, additive, clip to 1.0 | `applyContrast(bin, m_contrast)` + threshold `>0.05f` + normalise by `totalMag` | Brightness suppression |
| 3 | Scroll speed | Fixed per frame: `0.25 + 1.75*MOOD` LEDs/frame | Audio-modulated: `(0.5+mood*1.5) * (1+novelty*0.5)` | Speed jitter from novelty |
| 4 | Centre injection | Direct assignment: `leds_16[63] = leds_16[64] = colour` | EMA smoothing: `blendAlpha = 1 - exp(-dt/0.030)` | 30ms lag on colour changes |
| 5 | SQUARE_ITER | After clipping: `sum *= sum` per iteration | Missing — replaced by `applyContrast` | Different gain curve |
| 6 | Snapshot timing | AFTER centre inject, BEFORE edge fade | AFTER centre inject but only right half | Left half never enters scroll |
| 7 | Edge fade | Quadratic `(i/31)²` on outer 32 LEDs (¼ of strip) | Linear `(kHalf-1-i)/(kHalf-1)` on full right half | Over-aggressive darkening |
| 8 | Sqrt distortion | Not present in canonical SB | `sqrtf(prog)` spatial remap on right half | Creates banding artefact |
| 9 | Fallback colour | No fallback — silence = no injection | `if totalMag < 0.01 && rms > 0.02` → palette colour | Injects colour on silence |

## Architecture Decision: CRGB_F vs CRGB

**Critical constraint:** `drawSpriteScrolled` operates on `CRGB*` (uint8 0-255). V1 currently uses `CRGB_F*` (float 0.0-1.0) for its internal buffers because the SB port works in floating-point throughout.

**The correct approach:** Rewrite V1 to work in `CRGB` space for the scroll. The prism transform, bulb cover, and incandescent filter can remain in `CRGB_F` as post-processing steps AFTER the scroll output is captured.

The render flow becomes:

```
1. drawSpriteScrolled(ctx.leds, ...) — scroll the PREVIOUS frame outward (operates on ctx.leds directly)
2. Compute bloomColor from chromagram (stays in CRGB_F for precision)
3. Convert bloomColor to CRGB, write to ctx.leds[centreLeft] and ctx.leds[centreRight]
4. (Scroll state is now captured in ctx.leds — drawSpriteScrolled handles the snapshot internally)
5. Apply edge fade on ctx.leds (quadratic, outer quarter)
6. Mirror right half to left half on ctx.leds
7. Copy to CRGB_F workBuf for prism/bulb/incandescent post-processing (if enabled)
8. Convert back to ctx.leds
9. Mirror to strip 2
```

This means `prevBuf` (PSRAM) is NO LONGER NEEDED for the scroll — `drawSpriteScrolled` manages its own internal state via the scratch buffer. `workBuf` is still needed for prism post-processing.

## Task: Rewrite renderEffect()

### Step 1: Add the include

At the top of `SbK1BloomEffect.cpp`, add:

```cpp
#include "../../render/RenderPrimitives.h"
using lightwaveos::effects::render::drawSpriteScrolled;
```

### Step 2: Rewrite the scroll + colour synthesis + centre injection

Replace the current renderEffect body (lines 139-431) with a new implementation that follows canonical SB's per-frame equation:

**Canonical SB bloom per-frame (ground truth, from §2.4):**

```
1. Clear leds_16
2. draw_sprite(leds_16, leds_16_prev, 128, 128, position=0.25+1.75*MOOD, alpha=0.99)
3. Compute sum_color = Σ(i=0..11) hsv(i/12, SAT, chromagram[i]² * 1/6)
4. Clip sum_color to [0,1], then SQUARE_ITER squarings
5. force_saturation, optionally force_hue
6. Write centre pair: leds_16[63] = leds_16[64] = colour
7. Snapshot: leds_16_prev = leds_16  (BEFORE edge fade)
8. Quadratic edge fade on outer 32 LEDs
9. Mirror top half to bottom half
```

**Your implementation maps this to:**

```
1. (No manual clear — drawSpriteScrolled handles clear internally)
2. drawSpriteScrolled(ctx.leds, ctx.ledCount/2, centrePoint, scrollAmount, 0.99f, m_dt)
   where scrollAmount = (0.25f + 1.75f * m_mood) — FIXED per frame, NOT audio-modulated
   and centrePoint = kCenterRight (80)
   and the LED count is kStripLen (160) — ONE strip, not both
3. Compute bloomColor in CRGB_F: Σ chromaSmooth[i]² * (1/6) per bin
   NO threshold (remove the `> 0.05f` gate)
   NO normalise-by-totalMag (remove the `/= totalMag` block)
   Clip to [0,1] after accumulation
4. SQUARE_ITER: for (int s = 0; s < squareIter; ++s) { bloomColor.r *= bloomColor.r; ... }
   where squareIter = (int)m_contrast (reinterpret the contrast param as SQUARE_ITER)
5. force_saturation via existing HSV roundtrip (keep current code)
   force_hue in non-chromatic mode (keep current code)
6. Convert bloomColor to CRGB, write directly:
   ctx.leds[kCenterLeft] = bloomColor.toCRGB();
   ctx.leds[kCenterRight] = bloomColor.toCRGB();
   NO EMA smoothing (remove the blendAlpha/exp block — SB doesn't smooth)
7. (drawSpriteScrolled already captured the pre-scroll snapshot; no manual prevBuf copy needed)
8. Quadratic edge fade: for (i = 0; i < 32; ++i) {
     float fade = (float)i / 31.0f;
     fade = fade * fade;  // quadratic
     ctx.leds[kStripLen - 1 - i].nscale8(uint8_t(fade * 255.0f));
   }
   (Outer 32 LEDs = quarter of strip, matching SB's `i < 32`)
9. Mirror: for (i = 0; i < kHalf; ++i) ctx.leds[i] = ctx.leds[kStripLen - 1 - i];
```

### Step 3: Remove the sqrt distortion

The sqrt spatial remap (Step 8.5 in current code, lines 297-315) has NO canonical SB lineage. Remove it entirely.

### Step 4: Remove the fallback colour

The fallback colour injection (lines 248-252) has NO canonical SB lineage. Remove it.

### Step 5: Keep post-processing

The following post-processing steps should REMAIN but operate on `ctx.leds` converted to CRGB_F:

- **Prism transform** — copy `ctx.leds` to `workBuf` as CRGB_F, apply prism, blend back. Keep existing code structure.
- **Bulb cover** — keep.
- **Incandescent filter** — keep.

These features are K1-specific additions that don't affect the core bloom scroll behaviour.

### Step 6: Strip 2 mirror

After all post-processing, mirror strip 1 to strip 2:

```cpp
for (uint16_t i = 0; i < kStripLen && (kStripLen + i) < ledCount; ++i) {
    ctx.leds[kStripLen + i] = ctx.leds[i];
}
```

### Step 7: Clean up header

In `SbK1BloomEffect.h`:

- Remove `m_scrollAccum` — no longer needed; `drawSpriteScrolled` handles sub-pixel internally.
- `prevBuffer` in the PSRAM struct is no longer needed for the scroll. However, if prism transform still needs a scratch buffer, keep `workBuf` and `prismFxBuf` and `prismTmpBuf`. You may remove `prevBuffer` from the PSRAM struct if the prism code can work without it.
- Remove the `drawSprite` static method declaration — no longer used (replaced by `drawSpriteScrolled`).
- Update the algorithm description in the header comment to reflect the new flow.

### Step 8: Remove the old drawSprite method

In `SbK1BloomEffect.cpp`, remove the `drawSprite()` method (lines 87-106) entirely. It's replaced by `drawSpriteScrolled`.

## Key Differences from Current Code (checklist for the rewrite)

| Current V1 code | Canonical SB / new code | Why |
|---|---|---|
| `m_scrollAccum += scrollSpeed; int pixelsToScroll = (int)m_scrollAccum;` | `drawSpriteScrolled(..., 0.25f + 1.75f * m_mood, 0.99f, m_dt)` | Sub-pixel scroll, dt-corrected, no integer truncation |
| `scrollSpeed = (0.5+mood*1.5) * (1+novelty*0.5)` | `scrollAmount = 0.25f + 1.75f * m_mood` (constant per frame) | SB does NOT audio-modulate scroll speed |
| `if (bin > 0.05f)` threshold | No threshold — all 12 bins contribute | SB accumulates all bins |
| `bloomColor /= totalMag` normalisation | Clip to [0,1] after summation, NO normalisation | SB clips, doesn't normalise |
| `applyContrast(bin, m_contrast)` | `bin * bin * (1.0f/6.0f)` then SQUARE_ITER post-sum | SB squares pre-sum, then iterative squaring |
| EMA smoothing on centre: `blendAlpha = 1 - exp(-dt/0.030)` | Direct assignment: `leds[centre] = colour` | SB doesn't smooth centre injection |
| Linear edge fade over full half | Quadratic edge fade over outer 32 LEDs (quarter) | SB uses `(i/31)²` on outer quarter |
| `sqrtf(prog)` spatial distortion | Not present | No SB lineage |
| Fallback colour on silence | Not present | No SB lineage |

## What NOT To Do

1. Do NOT audio-modulate scroll speed. SB bloom scroll is `0.25 + 1.75*MOOD` — constant per frame, controlled by knob only.
2. Do NOT add threshold gates on chromagram bins. SB accumulates all 12 bins unconditionally.
3. Do NOT normalise by total magnitude. SB clips to [0,1], it does NOT divide by total.
4. Do NOT add EMA smoothing on the centre injection. SB writes directly.
5. Do NOT add sqrt spatial distortion. No SB lineage.
6. Do NOT add fallback colour on silence. No SB lineage.
7. Do NOT modify the base class, render primitives, or any other effect file.
8. Do NOT use `pio run -t upload` to flash.
9. Do NOT modify `PersistenceHelpers.h`.
10. Do NOT use `fadeToBlackByDt` in the new renderEffect. The alpha decay is handled inside `drawSpriteScrolled`.

## Build and Flash

After rewriting:

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz
```

Must succeed with zero errors.

Flash K1v2 using esptool-direct (read `~/.claude/projects/*/memory/feedback_k1v2_flash_procedure.md` for exact command — search all project memory directories if the file isn't at the expected path). Verify MAC before flash.

## Validation

**This is a visual regression test.** After flashing, Captain will test:

1. **K1 Bloom 0x1301** — should now show visible outward-scrolling trail from centre, coloured by chromagram, responsive to audio. The defining visual is a bloom of colour that expands outward from the centre and fades at the edges.
2. **0x1303 K1 Bloom V2** — control case, should still work (you haven't touched it).

## Report Format

```
PHASE 5 PROOF-OF-CONCEPT: K1 BLOOM V1 CANONICAL REWRITE

IMPLEMENTATION:
  Files modified: [list]
  Lines changed: [approx before/after for renderEffect]
  drawSpriteScrolled integration: [yes/no — describe how]
  Divergences resolved: [list which of the 9 are now fixed]
  
  Scroll: [describe new scroll mechanism]
  Colour synthesis: [describe — bin²*(1/6), no threshold, clip not normalise]
  Centre injection: [direct/EMA — should be direct]
  Edge fade: [quadratic/linear, what range]
  Sqrt distortion: [removed/kept]
  Fallback colour: [removed/kept]
  
  Post-processing retained: [prism/bulb/incandescent — yes/no for each]
  PSRAM usage: [what's still allocated, what was removed]

BUILD:
  Compilation: [success/failure]
  Warnings: [list any]
  
COMMIT:
  Hash: [commit hash]
  Message: [commit message]

FLASH:
  K1v2 flashed: [yes/no]
  Boot clean: [yes/no]

HARDWARE VALIDATION (Captain to perform):
  K1 Bloom 0x1301: [pending Captain visual test]
  K1 Bloom V2 0x1303: [pending — control case]

OPEN ISSUES: [any concerns, edge cases, potential problems]
```

## Confidence Gate (before commit)

1. `drawSpriteScrolled` is called with correct parameters? (scrollAmount = mood-scaled constant, alpha = 0.99, dt = m_dt)
2. Colour synthesis matches canonical SB? (bin²*(1/6), no threshold, clip not normalise, SQUARE_ITER)
3. Centre injection is direct assignment? (no EMA smoothing)
4. Edge fade is quadratic on outer 32 LEDs? (not linear over full half)
5. Sqrt distortion removed?
6. Fallback colour removed?
7. Build succeeds with zero errors?
8. Only SbK1BloomEffect.cpp and SbK1BloomEffect.h modified?
9. British English in all new comments?
10. No heap allocation in renderEffect()?
