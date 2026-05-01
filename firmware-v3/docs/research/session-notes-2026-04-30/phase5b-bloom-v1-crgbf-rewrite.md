# Phase 5B: K1 Bloom V1 (0x1301) — Canonical SB Rewrite in CRGB_F

## RBDO Gate

**GROUNDED** — premises traced to:
- Verbatim canonical SB 4.1.1 `light_mode_bloom()` (lightshow_modes.h:398-499, documented in canonical_SB_4_1_1.md §2.4)
- Canonical `draw_sprite()` (led_utilities.h:1247-1290, documented in §3.6)
- V1's EXISTING `drawSprite()` method (SbK1BloomEffect.cpp:87-106) — a faithful CRGB_F port of SB's draw_sprite, already present in the codebase but NOT USED by V1's scroll path
- Failed Phase 5 PoC proving uint8 CRGB precision in `drawSpriteScrolled` is inadequate for trail propagation (sub-1.0 brightness values truncate to 0; trail dies after ~4 LEDs)
- 9-divergence table from full source comparison
- Branch state: HEAD at ac413d33, failed PoC in uncommitted working tree

## Context

The Phase 5 PoC (previous prompt) failed because `drawSpriteScrolled` operates on `CRGB` (uint8 0-255). Trail propagation in sub-pixel scroll requires float precision — canonical SB uses `CRGB16` (SQ15x16 fixed-point, ~16-bit fractional precision). After alpha×sub-pixel weighting, uint8 values below 1.0 truncate to 0 and the trail dies within 3-4 LEDs.

**The irony:** V1 ALREADY HAS a perfectly good `drawSprite()` method operating on `CRGB_F*` (float 0.0-1.0) — a faithful port of SB's `draw_sprite()`. It does sub-pixel additive blitting with alpha decay at full float precision. But V1's `renderEffect()` NEVER CALLS IT for the scroll. Instead, renderEffect uses a broken integer-truncation scroll (lines 170-185) that wipes the trail when `pixelsToScroll` truncates to 0.

**This rewrite keeps V1's existing `drawSprite()` and rewrites `renderEffect()` to actually use it**, following canonical SB's per-frame algorithm exactly.

## Hard Constraints

- esptool-direct flash only (NOT `pio run -t upload`)
- Read K1v2 flash procedure from `~/.claude/projects/*/memory/` before flashing (search all project memory directories)
- Verify MAC (K1v2: `b4:3a:45:a5:87:f8`) before flash
- Centre origin (LED 79/80 outward), no heap in render(), 2.0ms ceiling, British English
- Do NOT modify any file other than `SbK1BloomEffect.cpp` and `SbK1BloomEffect.h`
- Do NOT modify the base class (`SbK1BaseEffect`)
- Do NOT modify RenderPrimitives.h/.cpp or FrameBlend.h/.cpp
- Do NOT modify PersistenceHelpers.h
- Do NOT use `drawSpriteScrolled` — the whole point is that uint8 precision is inadequate

## Task 0: Discard Failed PoC

The working tree has uncommitted changes from the failed Phase 5 PoC. Discard them:

```bash
cd /path/to/Lightwave-Ledstrip
git checkout HEAD -- firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BloomEffect.cpp
git checkout HEAD -- firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BloomEffect.h
```

Verify: `git diff HEAD -- firmware-v3/src/effects/ieffect/sensorybridge_reference/` should show NO changes.

**Do NOT** discard the other uncommitted files (docs/research additions) — those are unrelated and should be preserved.

## Reference Files (READ BEFORE CODING)

Read these in this exact order:

1. `firmware-v3/docs/research/spazz_redesign_2026-04-30/canonical_SB_4_1_1.md` §2.4 — verbatim `light_mode_bloom()` and per-frame motion equation. **THIS IS YOUR GROUND TRUTH.**
2. `firmware-v3/docs/research/spazz_redesign_2026-04-30/canonical_SB_4_1_1.md` §3.6 — verbatim `draw_sprite()`. **This is what V1's existing `drawSprite()` already implements.**
3. `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BloomEffect.cpp` lines 87-106 — V1's existing `drawSprite()`. **READ THIS. Confirm it matches SB's draw_sprite. You will USE this method, not replace it.**
4. `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BloomEffect.h` — PSRAM struct, constants, private methods, parameters.
5. `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BaseEffect.h` — CRGB_F type (lines 43-70), base class API, `m_chromaSmooth[]`, `m_huePosition`, `paletteColorF()`, `applyContrast()`.
6. `firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BaseEffect.cpp` — `baseProcessAudio()`.

## Canonical SB Bloom Algorithm (your implementation target)

From `lightshow_modes.h:398-499`, the per-frame algorithm is:

```
1.  memset(leds_16, 0, 128 * sizeof(CRGB16))             // Clear output buffer
2.  draw_sprite(leds_16, leds_16_prev, 128, 128,          // Scroll previous frame
               position = 0.25 + 1.75 * MOOD,            //   rightward by 0.25-2.0 LEDs
               alpha = 0.99)                               //   with 1% energy loss per frame
3.  sum_color = Σ(i=0..11) hsv(i/12, SAT, chroma[i]² / 6) // Chromagram colour synthesis
4.  Clip sum_color to [0,1]                                // Hard clip, NOT normalise
5.  for (s in 0..SQUARE_ITER): sum_color *= sum_color      // Iterative squaring (soft-knee gain)
6.  force_saturation(sum_color, SAT)                       // Vivid colours
7.  if !chromatic: force_hue(sum_color, hue_position)      // Non-chromatic mode
8.  leds_16[63] = leds_16[64] = sum_color                  // Direct centre injection (NO smoothing)
9.  leds_16_prev = leds_16                                 // Full snapshot BEFORE edge fade
10. for i=0..31: leds_16[127-i] *= (i/31)²                // Quadratic edge fade, outer half of right side
11. for i=0..63: leds_16[i] = leds_16[127-i]              // Mirror right→left
```

**Critical architectural note on the scroll:** SB shifts the ENTIRE buffer rightward by `position` LEDs. It is a UNIDIRECTIONAL shift, not bidirectional. The "outward from centre" visual appearance comes from:
- Centre injection constantly feeding fresh colour at indices 63/64
- The rightward shift carrying that colour toward the right edge
- The mirror at step 11 creating the left-side reflection
- The edge fade at step 10 killing content at the boundary

The left half's scrolled content is irrelevant — it gets overwritten by the mirror. This is simpler and more correct than bidirectional centre-origin scroll.

## Task 1: Rewrite renderEffect()

Replace the ENTIRE body of `renderEffect()` (everything between the `#if FEATURE_AUDIO_SYNC` / audio availability check and the `#endif` blocks). Keep the NATIVE_BUILD and FEATURE_AUDIO_SYNC guards and the `fadeToBlackByDt` on no-audio path.

### New renderEffect implementation:

```cpp
void SbK1BloomEffect::renderEffect(plugins::EffectContext& ctx) {
#ifndef NATIVE_BUILD
    if (!m_bloom) return;
#else
    (void)ctx;
    return;
#endif

#ifndef NATIVE_BUILD
#if !FEATURE_AUDIO_SYNC
    (void)ctx;
    return;
#else
    if (!ctx.audio.available) {
        fadeToBlackByDt(ctx.leds, ctx.ledCount, 32, ctx.getSafeDeltaSeconds());
        return;
    }

    CRGB_F* workBuf = m_bloom->workBuffer;
    CRGB_F* prevBuf = m_bloom->prevBuffer;

    // ─────────────────────────────────────────────────────────────────
    // Canonical SB 4.1.1 light_mode_bloom port.
    // Scroll runs in CRGB_F space via the existing drawSprite() method
    // (faithful port of SB's draw_sprite, led_utilities.h:1247-1290).
    // Float precision preserves sub-1.0 brightness across propagation
    // steps — the reason uint8 drawSpriteScrolled failed.
    // ─────────────────────────────────────────────────────────────────

    // Step 1: Clear working buffer
    std::memset(workBuf, 0, kStripLen * sizeof(CRGB_F));

    // Step 2: Scroll previous frame rightward via sub-pixel sprite blit.
    //   SB canonical: draw_sprite(dest, prev, 128, 128,
    //                             position = 0.25 + 1.75*MOOD, alpha = 0.99)
    //   Unidirectional rightward shift; left half gets overwritten by
    //   mirror at step 11. Frame-coupled (not dt-corrected) to match
    //   canonical SB which runs at a fixed frame rate.
    const float scrollPosition = 0.25f + 1.75f * m_mood;
    drawSprite(workBuf, prevBuf, kStripLen, kStripLen,
               scrollPosition, 0.99f);

    // Step 3: Chromagram colour synthesis.
    //   SB canonical: sum_color = Σ hsv(i/12, SAT, chroma[i]² * 1/6)
    //   NO threshold gate. NO totalMag normalisation. Clip after sum.
    const float kShare = 1.0f / 6.0f;
    CRGB_F bloomColor = {0.0f, 0.0f, 0.0f};
    for (uint8_t c = 0; c < 12; ++c) {
        float bin = m_chromaSmooth[c];
        float val = bin * bin * kShare;  // SB: bin² * (1/6)

        float prog = c / 12.0f;
        float palPos = prog + 0.5f;  // Bloom-specific: cyan offset
        const bool chromaticMode = (ctx.saturation >= 128);
        if (chromaticMode) {
            palPos += m_huePosition;
        }

        // Palette lookup at bin-squared brightness, then accumulate
        CRGB_F noteColor = paletteColorF(ctx.palette, palPos, val);
        bloomColor += noteColor;
    }

    // Step 4: Clip to [0,1] — SB clips, does NOT normalise
    bloomColor.clip();

    // Step 5: SQUARE_ITER — iterative squaring (soft-knee gain curve).
    //   SB: for (i = 0; i < CONFIG.SQUARE_ITER; i++) { sum *= sum; }
    //   Map m_contrast (0.0-3.0, default 1.0) to integer iteration count.
    const int squareIter = static_cast<int>(m_contrast);
    for (int s = 0; s < squareIter; ++s) {
        bloomColor.r *= bloomColor.r;
        bloomColor.g *= bloomColor.g;
        bloomColor.b *= bloomColor.b;
    }

    // Step 6: force_saturation — ensure vivid colours
#ifndef NATIVE_BUILD
    {
        CRGB tempRgb = bloomColor.toCRGB();
        CHSV tempHsv = rgb2hsv_approximate(tempRgb);
        tempHsv.s = ctx.saturation;
        hsv2rgb_rainbow(tempHsv, tempRgb);
        bloomColor = CRGB_F::fromCRGB(tempRgb);
    }
#endif

    // Step 7: Non-chromatic mode — force hue from CHROMA knob
    //   SB: force_hue(temp_col, 255 * (chroma_val + hue_position))
    const bool chromaticMode = (ctx.saturation >= 128);
    if (!chromaticMode) {
        float maxComp = fmaxf(bloomColor.r, fmaxf(bloomColor.g, bloomColor.b));
        if (maxComp < 0.001f) maxComp = 0.001f;
        float forcedPos = m_chromaHue + m_huePosition;
        bloomColor = paletteColorF(ctx.palette, forcedPos, fminf(maxComp, 1.0f));
    }

    // Apply PHOTONS brightness
    const float photons = static_cast<float>(ctx.brightness) / 255.0f;
    bloomColor *= photons;
    bloomColor.clip();

    // Step 8: Direct centre injection — NO EMA smoothing.
    //   SB: leds_16[63] = leds_16[64] = colour (direct assignment)
    workBuf[kCenterLeft]  = bloomColor;
    workBuf[kCenterRight] = bloomColor;

    // Step 9: Snapshot FULL buffer to prevBuf BEFORE edge fade.
    //   SB: memcpy(leds_16_prev, leds_16, sizeof * NATIVE_RESOLUTION)
    //   Must be BEFORE edge fade so the quadratic taper doesn't
    //   accumulate into the scroll state across frames.
    std::memcpy(prevBuf, workBuf, kStripLen * sizeof(CRGB_F));

    // Step 10: Quadratic edge fade on outer half of right side.
    //   SB: for i=0..31: leds_16[127-i] *= (i/31)²
    //   SB fades 32 of 64 LEDs in the right half (50% of one side).
    //   K1 equivalent: 40 of 80 LEDs in the right half.
    static constexpr uint16_t kEdgeFadeCount = kHalf / 2;  // 40
    for (uint16_t i = 0; i < kEdgeFadeCount; ++i) {
        float prog = static_cast<float>(i) / static_cast<float>(kEdgeFadeCount - 1);
        float fade = prog * prog;  // Quadratic
        workBuf[kStripLen - 1 - i] *= fade;
    }

    // Step 11: Mirror right half to left half.
    //   SB: for i=0..63: leds_16[i] = leds_16[127-i]
    for (uint16_t i = 0; i < kHalf; ++i) {
        workBuf[kCenterLeft - i] = workBuf[kCenterRight + i];
    }

    // ─────────────────────────────────────────────────────────────────
    // K1-specific post-processing (not in canonical SB, but adds value)
    // ─────────────────────────────────────────────────────────────────

    // ... [KEEP existing prism, bulb cover, incandescent code UNCHANGED] ...

    // Convert to ctx.leds and mirror to strip 2
    const uint16_t ledCount = ctx.ledCount;
    for (uint16_t i = 0; i < kStripLen && i < ledCount; ++i) {
        ctx.leds[i] = workBuf[i].toCRGB();
    }
    for (uint16_t i = 0; i < kStripLen && (kStripLen + i) < ledCount; ++i) {
        ctx.leds[kStripLen + i] = ctx.leds[i];
    }

#endif // FEATURE_AUDIO_SYNC
#endif // NATIVE_BUILD
}
```

### What to keep from the original renderEffect UNCHANGED:

- The `#ifndef NATIVE_BUILD` / `#if !FEATURE_AUDIO_SYNC` guard structure
- The `fadeToBlackByDt` on no-audio path
- The prism effect block (lines 336-383 in original) — copy it verbatim into the new renderEffect after step 11
- The bulb cover block (lines 389-399 in original) — copy verbatim
- The incandescent filter block (lines 404-415 in original) — copy verbatim
- The strip 2 mirror (lines 420-427 in original) — copy verbatim

### What to REMOVE from renderEffect:

| Removed code | Original lines | Why |
|---|---|---|
| Integer scroll accumulator | 170-185 | Replaced by drawSprite() call with sub-pixel position |
| `bin > 0.05f` threshold | 201 | No SB lineage — SB accumulates ALL 12 bins |
| `applyContrast(bin, m_contrast)` | 202 | Replaced by `bin * bin * kShare` (canonical SB formula) |
| `totalMag` accumulation + normalisation | 199, 218, 226-230 | No SB lineage — SB clips, doesn't normalise |
| Fallback colour on silence | 248-252 | No SB lineage |
| EMA centre smoothing | 279-283 | No SB lineage — SB writes directly |
| Right-half-only snapshot | 290 | SB snapshots FULL buffer |
| Sqrt spatial distortion | 297-315 | No SB lineage |
| Linear full-half edge fade | 320-323 | Replaced by quadratic edge fade on outer half of right side |

### What to KEEP outside renderEffect:

- `drawSprite()` method (lines 87-106) — **THIS IS THE KEY FUNCTION. Keep it exactly as-is.**
- `prismTransform()` method (lines 112-133) — unchanged
- All lifecycle code (init, cleanup) — unchanged
- All parameter code — unchanged
- All metadata code — unchanged

## Task 2: Clean up header (SbK1BloomEffect.h)

Remove `m_scrollAccum` member variable (line 84 in original). It's no longer used — `drawSprite` takes a fixed `position` parameter per frame.

Update the algorithm description comment (lines 8-21) to reflect the new flow:

```
 * Key algorithm steps (canonical SB 4.1.1 light_mode_bloom port):
 * 1. Clear working buffer (CRGB_F, float precision)
 * 2. Sub-pixel scroll previous frame rightward via drawSprite (CRGB_F)
 * 3. Synthesize colour from 12-bin chromagram (bin² × 1/6, additive, clip)
 * 4. SQUARE_ITER iterative squaring (soft-knee gain)
 * 5. Force saturation, optionally force hue
 * 6. Direct inject at centre pair (LEDs 79/80)
 * 7. Snapshot full buffer to scroll state (before edge fade)
 * 8. Quadratic edge fade on outer half of right side
 * 9. Mirror right half to left half
 * 10. K1 post-processing: prism, bulb cover, incandescent filter
 * 11. Output to ctx.leds (both strips)
```

Everything else in the header stays unchanged. The PSRAM struct stays as-is (workBuffer, prevBuffer, prismFxBuf, prismTmpBuf all still used).

## Task 3: Build

```bash
cd firmware-v3
pio run -e esp32dev_audio_esv11_k1v2_32khz
```

Must succeed with zero errors. Note any warnings.

## Task 4: Commit

```bash
cd /path/to/Lightwave-Ledstrip
git add firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BloomEffect.cpp
git add firmware-v3/src/effects/ieffect/sensorybridge_reference/SbK1BloomEffect.h
```

Verify `git diff --cached --stat` shows ONLY these 2 files. Nothing else.

Commit message:
```
fix(effects): 0x1301 K1 Bloom — canonical SB 4.1.1 rewrite in CRGB_F

Rewrites renderEffect() to follow canonical SB light_mode_bloom()
(lightshow_modes.h:398-499) step-by-step in CRGB_F float precision.

Root cause of "almost static": V1 had a drawSprite() method (faithful
SB draw_sprite port, CRGB_F sub-pixel additive blit) but renderEffect
never called it — used integer scroll truncation instead. When
pixelsToScroll truncated to 0 (~26% of frames), trail wiped entirely.

Phase 5 PoC (drawSpriteScrolled, uint8) also failed: sub-pixel weight
× alpha produces values < 1.0 which truncate to 0 in CRGB. Trail died
after ~4 LEDs. CRGB_F preserves float precision across propagation.

Changes vs the broken V1:
- Scroll: drawSprite(workBuf, prevBuf, ...) with float precision
- Scroll speed: 0.25+1.75*MOOD (SB canonical, not audio-modulated)
- Colour: bin²*(1/6) + clip (SB canonical, no threshold/normalisation)
- Centre inject: direct assignment (SB canonical, no EMA smoothing)
- Edge fade: quadratic on outer half of right side (SB canonical)
- Snapshot: full buffer before edge fade (SB canonical)
- Removed: sqrt distortion, fallback colour (no SB lineage)
- Kept: prism, bulb cover, incandescent filter (K1 additions)
```

## Task 5: Flash K1v2

Read K1v2 flash procedure from `~/.claude/projects/*/memory/` (search for `flash_procedure` or `k1v2_flash` or similar). Verify MAC before flash. Use esptool-direct only.

## Report Format

```
PHASE 5B: K1 BLOOM V1 CANONICAL REWRITE (CRGB_F)

TASK 0: DISCARD FAILED PoC
  Files restored: [yes/no]
  Working tree clean for SbK1Bloom*: [yes/no]

TASK 1: REWRITE
  drawSprite() kept: [yes — describe it's used for the scroll now]
  Scroll mechanism: [drawSprite(workBuf, prevBuf, 160, 160, 0.25+1.75*mood, 0.99)]
  Colour synthesis: [bin²*(1/6), no threshold, clip not normalise, SQUARE_ITER post-sum]
  Centre injection: [direct assignment, no EMA]
  Edge fade: [quadratic, outer 40 LEDs of right half]
  Sqrt distortion: [removed]
  Fallback colour: [removed]
  Post-processing: [prism: kept / bulb: kept / incandescent: kept]

  Divergences resolved (from the 9-table):
    #1 integer scroll → drawSprite sub-pixel: [yes/no]
    #2 threshold+normalise → bin²*(1/6)+clip: [yes/no]
    #3 audio-modulated speed → mood-only: [yes/no]
    #4 EMA centre → direct inject: [yes/no]
    #5 applyContrast → SQUARE_ITER: [yes/no]
    #6 right-half snapshot → full snapshot: [yes/no]
    #7 linear full-half fade → quadratic outer-half: [yes/no]
    #8 sqrt distortion → removed: [yes/no]
    #9 fallback colour → removed: [yes/no]

TASK 2: HEADER CLEANUP
  m_scrollAccum removed: [yes/no]
  Algorithm comment updated: [yes/no]

TASK 3: BUILD
  Compilation: [success/failure]
  Warnings: [list any]

TASK 4: COMMIT
  Hash: [commit hash]
  Files committed: [list — should be exactly 2]

TASK 5: FLASH
  K1v2 flashed: [yes/no — if no, why]
  Boot clean: [yes/no]

HARDWARE VALIDATION (Captain to perform):
  K1 Bloom 0x1301: [pending — should show outward-scrolling trail from centre]
  K1 Bloom V2 0x1303: [pending — control case, should be unchanged]

OPEN ISSUES: [any]
```

## What NOT To Do

1. Do NOT use `drawSpriteScrolled` (uint8 precision is inadequate for trail propagation).
2. Do NOT delete V1's existing `drawSprite()` method — that IS the scroll function.
3. Do NOT audio-modulate scroll speed. SB bloom uses `0.25 + 1.75*MOOD` — fixed per frame.
4. Do NOT add a threshold gate on chromagram bins. SB accumulates all 12.
5. Do NOT normalise by total magnitude. SB clips to [0,1].
6. Do NOT add EMA smoothing on centre injection. SB writes directly.
7. Do NOT add sqrt spatial distortion. No SB lineage.
8. Do NOT add fallback colour on silence. No SB lineage.
9. Do NOT dt-correct the scroll amount or alpha. Frame-coupled to match canonical SB.
10. Do NOT modify any file other than SbK1BloomEffect.cpp and SbK1BloomEffect.h.
11. Do NOT use `pio run -t upload` to flash.
12. Do NOT touch the base class, render primitives, PersistenceHelpers, or any other effect.

## Confidence Gate (before commit)

1. `drawSprite()` method is KEPT and CALLED by renderEffect()? (Not deleted, not replaced)
2. drawSprite called with: `(workBuf, prevBuf, kStripLen, kStripLen, scrollPosition, 0.99f)`?
3. scrollPosition = `0.25f + 1.75f * m_mood`? (Not audio-modulated)
4. Colour synthesis: `bin * bin * (1.0f/6.0f)` per chroma bin? (Not applyContrast)
5. No threshold gate on chroma bins? (All 12 contribute)
6. Post-sum clip to [0,1], then SQUARE_ITER squaring? (Not totalMag normalisation)
7. Centre injection is direct assignment to workBuf[kCenterLeft] and workBuf[kCenterRight]? (No EMA)
8. Full buffer snapshot (prevBuf = workBuf) BEFORE edge fade?
9. Edge fade is quadratic on outer kHalf/2 (40) LEDs of right side?
10. Sqrt distortion removed?
11. Fallback colour removed?
12. m_scrollAccum removed from header?
13. Build succeeds with zero errors?
14. Only SbK1BloomEffect.cpp and SbK1BloomEffect.h staged for commit?
15. British English in all new/modified comments?
16. No heap allocation in renderEffect?
