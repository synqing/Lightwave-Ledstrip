/**
 * @file RenderPrimitives.h
 * @brief Layer 4 — context-free render primitives for centre-origin LED strips.
 *
 * Pipeline-reform Phase 1 deliverable per
 * `firmware-v3/docs/research/spazz_redesign_2026-04-30/PIPELINE_REFORM.md` §2.
 *
 * Three primitives, three motion laws:
 *   - drawDot              — discrete dot with optional motion-blur trail
 *                            (Emotiscope `draw_dot` lineage; SB 4.1.1
 *                             `light_mode_vu_dot` and `light_mode_chromagram_dots`)
 *   - drawSpriteScrolled   — additive sub-pixel buffer scroll with
 *                            multiplicative fade (Emotiscope `draw_sprite`;
 *                            SB 4.1.1 `light_mode_bloom`). The wave-propagation
 *                            primitive.
 *   - fillFromBins         — direct centre-origin spectral fill
 *                            (SB 4.1.1 `light_mode_gdft`,
 *                             `light_mode_chromagram_gradient`).
 *
 * Design contract (binding):
 *   - Context-free: take raw `CRGB* leds`, `ledCount`, `centrePoint`. No
 *     EffectContext / RenderContext coupling. Effect base classes (Phase 4)
 *     extract the relevant context fields and pass them through.
 *   - Centre-origin: position/scroll are referenced about LED `centrePoint - 1`
 *     (left of centre) and `centrePoint` (right of centre). For K1 v2 with
 *     ledCount=160, centrePoint=80 the centre pair is LEDs 79+80 and the edges
 *     are LEDs 0 and 159, matching the project doctrine.
 *   - dt-correct: persistence and trail terms are exponentiated over a 120-FPS
 *     reference (`powf(rate, dt * 120)`) so behaviour is frame-rate independent.
 *     Mirrors `lightwaveos::effects::persistence::dtDecay` convention.
 *   - No heap allocation in any function transitively reachable from render().
 *     A single file-scope scratch buffer in `RenderPrimitives.cpp` services
 *     `drawSpriteScrolled`'s in-place scatter; sized for K1's largest frame
 *     (320 LEDs).
 *   - Bounds-checked. Out-of-range writes are silently dropped, never wrap or
 *     stomp neighbouring memory.
 *
 * Performance targets (per primitive, 320 LEDs, ESP32-S3 @ 240 MHz):
 *   - drawDot              < 0.1 ms
 *   - drawSpriteScrolled   < 0.3 ms (single-pass scatter)
 *   - fillFromBins         < 0.1 ms
 * All well under the 2.0 ms render-frame ceiling.
 *
 * British English in comments and identifiers (centre, colour, behaviour).
 */

#pragma once

#include <cstdint>

#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace render {

/**
 * @brief Draw a sub-pixel positioned dot with optional motion-blur trail.
 *
 * Position is in normalised space:
 *   0.0 = centre pair (LEDs `centrePoint-1` + `centrePoint`)
 *   1.0 = edges       (LEDs 0 and `ledCount-1`)
 *
 * If `prevPosition >= 0`, the function draws an additive line from
 * `prevPosition` to `position` at a brightness inversely proportional to the
 * swept distance (Emotiscope's `1 / spread_area` law). Stationary dots are
 * fully bright at one pixel; fast-moving dots are dim and spread across the
 * sweep. Pass -1.0f to disable the trail and draw a single sub-pixel dot.
 *
 * Reference: Emotiscope-1.2 `draw_dot()` in `src/leds.h`; SB-4.1.1
 * `draw_dot()` / `draw_line()` in `led_utilities.h`.
 *
 * @param leds         LED buffer to write to (additive; existing content is
 *                     preserved, not overwritten — pre-clear the buffer if a
 *                     hard write is required).
 * @param ledCount     Number of LEDs in `leds[]`.
 * @param centrePoint  Right-of-centre LED index (80 for K1 v2; the centre pair
 *                     is `centrePoint-1` and `centrePoint`).
 * @param position     Normalised position in [0, 1]; clamped internally.
 * @param colour       Colour to additively blend.
 * @param opacity      Brightness scalar in [0, 1]; clamped internally.
 * @param prevPosition Previous frame's normalised position for the motion-blur
 *                     trail; -1.0f (default) disables the trail.
 * @param mirror       If true, mirror the dot symmetrically across the centre
 *                     pair (default: true). Set false for asymmetric effects.
 */
void drawDot(CRGB* leds,
             uint16_t ledCount,
             uint16_t centrePoint,
             float position,
             CRGB colour,
             float opacity,
             float prevPosition = -1.0f,
             bool mirror = true);

/**
 * @brief Additive centre-origin sub-pixel buffer scroll with multiplicative fade.
 *
 * Scrolls existing buffer content outward from the centre pair by
 * `scrollAmount` LEDs per call, fading by `alpha`. This is the wave-propagation
 * primitive — every Bloom / wave / collision effect is a thin shim over it.
 *
 * Behaviour per call:
 *   1. Snapshot `leds[]` into a file-scope scratch buffer.
 *   2. Clear `leds[]`.
 *   3. For each scratch pixel, scatter it additively into `leds[]` at the
 *      sub-pixel destination (i ± scrollAmount, depending on whether i is to
 *      the right or left of centrePoint), weighted by `alpha`.
 *
 * `scrollAmount` is pre-scaled by dt by the caller (i.e. caller passes
 * `pxPerSecond * dt`). `alpha` is dt-corrected internally as
 * `powf(alpha, dt * 120)` so a steady-state alpha of 0.99 at 120 FPS produces
 * the same visual decay rate at 60 FPS or 240 FPS.
 *
 * Reference: Emotiscope-1.2 `draw_sprite()` in `src/leds.h` (additive sub-pixel
 * scatter with bilinear blend); SB-4.1.1 `light_mode_bloom` use of
 * `draw_sprite(... position = 0.25 + 1.75*MOOD, alpha = 0.99)`.
 *
 * @param leds         LED buffer (read-modify-write in place).
 * @param ledCount     Number of LEDs in `leds[]` (must be ≤ 320).
 * @param centrePoint  Right-of-centre LED index (80 for K1 v2).
 * @param scrollAmount Fractional LEDs to move outward from centre per call,
 *                     pre-scaled by dt. Negative values scroll inward.
 * @param alpha        Per-frame multiplicative survival rate at 120 FPS
 *                     reference (0 = instant clear, 1 = no fade, typical
 *                     0.90–0.99). dt-corrected internally.
 * @param dt           Frame time in seconds (use ctx.getSafeDeltaSeconds()).
 */
void drawSpriteScrolled(CRGB* leds,
                        uint16_t ledCount,
                        uint16_t centrePoint,
                        float scrollAmount,
                        float alpha,
                        float dt);

/**
 * @brief Map frequency bins to LEDs centre-origin with palette-derived colours.
 *
 * Bin 0 maps to the centre pair (LEDs `centrePoint-1` and `centrePoint`).
 * Bin `binCount-1` maps to the edges (LEDs 0 and `ledCount-1`). Intermediate
 * bins are placed proportionally; in-between LEDs are left alone (no
 * interpolation across LEDs — that is the caller's choice via post-blur or
 * additive overlap).
 *
 * Each bin's colour is sampled from `palette` at `paletteIdx = b/(binCount-1) * 255`
 * and scaled by the bin magnitude (clamped to [0,1]) × `brightness`.
 *
 * Reference: SB-4.1.1 `light_mode_gdft`, `light_mode_chromagram_gradient`,
 * `light_mode_octave` per the canonical audit.
 *
 * @param leds         LED buffer.
 * @param ledCount     Number of LEDs.
 * @param centrePoint  Right-of-centre LED index.
 * @param bins         Array of bin magnitudes in [0, 1] (clamped internally).
 * @param binCount     Number of bins (1–64 typical; 8 for K1 octave bands,
 *                     12 for chromagram, 64 for SB-style spectrogram).
 * @param palette      Colour palette; bin index → palette index linearly.
 * @param brightness   Master brightness 0–255.
 * @param additive     If true, additively blend onto existing buffer; if false
 *                     (default), overwrite. Additive mode is useful when
 *                     compositing a spectrogram on top of a scrolled bloom.
 */
void fillFromBins(CRGB* leds,
                  uint16_t ledCount,
                  uint16_t centrePoint,
                  const float* bins,
                  uint8_t binCount,
                  const CRGBPalette16& palette,
                  uint8_t brightness,
                  bool additive = false);

}  // namespace render
}  // namespace effects
}  // namespace lightwaveos
