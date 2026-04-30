/**
 * @file FrameBlend.h
 * @brief Layer 5 — whole-image one-pole IIR frame post-process.
 *
 * Pipeline-reform Phase 2 deliverable per
 * `firmware-v3/docs/research/spazz_redesign_2026-04-30/PIPELINE_REFORM.md` §2.
 *
 * `applyFrameBlending` replaces per-effect `fadeToBlackByDt(K<255)` motion
 * mechanisms with a single mood-controlled blend across the whole frame
 * buffer. Reference: Emotiscope-1.2 `apply_frame_blending()` in `src/leds.h`
 * (whole-image LPF after all modes have rendered, before `FastLED.show()`).
 *
 * Integration point (Phase 5 — NOT this deliverable): RendererActor calls this
 * once per frame, after all effects + zone composition complete and before
 * `FastLED.show()`. The previous-frame buffer is owned by the caller (one
 * static allocation in RendererActor; ~960 bytes for a 320-LED strip).
 *
 * Design contract (binding):
 *   - Persistence is mood-driven: mood=0 → no blend (snappy, single-frame);
 *     mood=255 → blendCoeff = 0.92 (heavy persistence, soft trails).
 *   - dt-correct: blend coefficient is exponentiated over a 120-FPS reference
 *     so a steady-state mood produces frame-rate-independent visual decay.
 *   - In-place. `leds[]` is read AND written; `prevFrame[]` is updated at the
 *     end so the caller can call again next frame with the same buffers.
 *   - Pre-clears `prevFrame[]` to current frame when mood == 0; preserves
 *     buffer continuity across mood changes.
 *   - No heap allocation. Caller owns both buffers.
 *
 * British English in comments and identifiers.
 */

#pragma once

#include <cstdint>

#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace render {

/**
 * @brief Whole-image one-pole IIR blend, mood-controlled persistence.
 *
 * Per-pixel formula (all per-pixel ops, no global state):
 *   blendCoeff   = (mood / 255) × 0.92
 *   dtCorrected  = blendCoeff ^ (dt × 120)
 *   leds[i]      = prevFrame[i] × dtCorrected + leds[i] × (1 - dtCorrected)
 *   prevFrame[i] = leds[i]                           (for next frame)
 *
 * mood=0 short-circuits the blend (output equals input verbatim) and updates
 * `prevFrame` with the input so transitioning to mood>0 is seamless.
 *
 * @param leds       Current frame buffer (read + written in place).
 * @param prevFrame  Previous frame buffer (read + written in place; caller-owned;
 *                   typically a static array in RendererActor).
 * @param ledCount   Number of LEDs in both buffers.
 * @param mood       Persistence knob (0–255). 0 = no blend, 255 = soft trails.
 * @param dt         Frame interval in seconds (use ctx.getSafeDeltaSeconds()).
 */
void applyFrameBlending(CRGB* leds,
                        CRGB* prevFrame,
                        uint16_t ledCount,
                        uint8_t mood,
                        float dt);

}  // namespace render
}  // namespace effects
}  // namespace lightwaveos
