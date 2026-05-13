/**
 * @file FramebufferLPF.h
 * @brief Dt-correct global framebuffer low-pass filter (image LPF).
 *
 * Phase 1 Move 1.3 of the K1 visual-pipeline synergy-topology kill order
 * (INF-02). Lineage: Emotiscope/SensoryBridge `apply_image_lpf` (gpu_core.h:87)
 * — a per-frame elementwise EMA over the rendered framebuffer that smooths
 * harsh per-frame transitions without blurring spatial detail.
 *
 * The cutoff curve is taken verbatim from the ES reference:
 *
 *   cutoffHz = 0.5 + (1 - sqrt(softness01)) * 14.5
 *
 * with `softness01 = 0` mapping to a sharp 15 Hz response (responsive,
 * minimal smoothing) and `softness01 = 1` mapping to a fluid 0.5 Hz
 * response (heavy smoothing, persistent afterimage). The square root
 * curve gives a perceptually linear knob — small movements near the sharp
 * end of the dial produce noticeable changes, mirroring Emotiscope's
 * "softness" feel.
 *
 * Per Topology_Reconciliation §3 C-2, multiplicativity of the persistence
 * substrate requires per-layer τ exposure: callers may bypass the softness
 * curve and set the cutoff directly via `setCutoffHz()`. Both APIs are
 * available so a mode preset can keep the perceptual softness knob while
 * a layer composer can inject distinct cutoffs per layer.
 *
 * ── Algorithm ────────────────────────────────────────────────────────────
 *
 * For each LED i, the filter holds the previous frame's RGB and computes:
 *
 *   alpha = 1 - exp(-2π · cutoffHz · dt)
 *   out[i] = prev[i] · (1 - alpha) + new[i] · alpha
 *   prev[i] = out[i]    // becomes input to next frame
 *
 * The new value is the framebuffer the caller has just rendered — `apply()`
 * mutates the destination in place. State is held internally in a static
 * 320 × CRGB buffer (1280 bytes) — no heap allocation, no PSRAM call,
 * lifetime tied to the caller-owned LPF instance.
 *
 * ── Constraints ──────────────────────────────────────────────────────────
 *
 *  - NO heap allocation. State is a fixed-size struct member; `apply()`
 *    and `reset()` never touch the heap.
 *  - dt-correct via `1 - exp(-2π · cutoffHz · dt)`. Behaviour is identical
 *    at 60 Hz, 120 Hz, or under variable frame pacing — the same cutoff
 *    yields the same physical response time regardless of frame rate.
 *  - Geometry-agnostic. Per-LED EMA respects whatever centre-origin /
 *    mirrored / zone-restricted layout the caller has rendered.
 *  - Cheap. ~50 µs at 320 LEDs on ESP32-S3 (one expf, one loop with three
 *    8-bit MACs per pixel) — well inside the 2 ms render budget.
 *  - British English in comments and identifiers (centre, colour, etc.).
 *
 * ── Usage ────────────────────────────────────────────────────────────────
 *
 *   FramebufferLPF lpf;
 *   lpf.setSoftness(0.4f);        // perceptual knob, ES-style
 *   // ... render frame into ledBuf ...
 *   lpf.apply(ledBuf, dt);        // mutates ledBuf in place
 *
 * Or for per-layer τ exposure:
 *
 *   layerLpf.setCutoffHz(2.5f);   // direct cutoff for this layer
 *   layerLpf.apply(layerBuf, dt);
 *
 * Phase 4 of integration (RendererActor mandatory pass) is a separate
 * later move — this file delivers the helper and TDD only.
 */

#pragma once

#include <cmath>

#include <FastLED.h>

#include "effects/PerceptualJND.h"

namespace lightwaveos {
namespace effects {
namespace persistence {

/// @brief Total framebuffer length (2 × 160 LED dual strip = 320 LEDs).
static constexpr int kFramebufferLen = 320;
using lightwaveos::effects::perceptual::kFramebufferLpfMinimumCutoffHz;

class FramebufferLPF {
public:
    /// @brief Configure cutoff via softness knob (Emotiscope curve).
    ///
    /// `softness01` is clamped to [0, 1]. Mapping:
    ///   - 0.0 → cutoffHz = 15.0 (sharp, minimal smoothing)
    ///   - 1.0 → cutoffHz = 0.5  (fluid, heavy smoothing)
    /// Curve: `cutoffHz = 0.5 + (1 - sqrt(softness01)) · 14.5`.
    inline void setSoftness(float softness01) {
        if (softness01 < 0.0f) softness01 = 0.0f;
        if (softness01 > 1.0f) softness01 = 1.0f;
        cutoffHz_ = kFramebufferLpfMinimumCutoffHz + (1.0f - sqrtf(softness01)) * 14.5f;
    }

    /// @brief Direct cutoff override (bypasses the softness curve).
    ///
    /// Per Topology_Reconciliation §3 C-2, per-layer τ exposure is
    /// required for the persistence substrate to compose multiplicatively
    /// with downstream layers. `cutoffHz` is clamped to a non-negative
    /// value; pass 0 to freeze the prev frame, large values to disable
    /// smoothing.
    inline void setCutoffHz(float cutoffHz) {
        cutoffHz_ = (cutoffHz < 0.0f) ? 0.0f : cutoffHz;
    }

    /// @brief Current cutoff frequency in Hz (read-only accessor).
    inline float cutoffHz() const { return cutoffHz_; }

    /// @brief Apply the LPF to `dest[0 .. kFramebufferLen)` in place.
    ///
    /// `dt` is the frame interval in seconds. On the first call after
    /// construction or `reset()`, `prevFrame_` is zero, so a frame
    /// rendered against pure black darkens proportionally to (1 - alpha).
    ///
    /// `dest == nullptr` is a no-op for safety. `dt <= 0` is treated as
    /// zero — the filter holds its previous state.
    inline void apply(CRGB* dest, float dt) {
        if (dest == nullptr) {
            return;
        }
        if (dt < 0.0f) dt = 0.0f;
        // alpha = 1 - exp(-2π · cutoffHz · dt) — dt-correct EMA weight.
        const float twoPi = 6.283185307179586f;
        const float alpha = 1.0f - expf(-twoPi * cutoffHz_ * dt);
        // Multiply by 256 once for fixed-point CRGB blends; expressed in
        // float to keep the rounding behaviour identical to the ES
        // reference. We cast back to uint8_t at the end — channel range
        // is implicitly [0, 255] because both inputs are in that range.
        const float oneMinusAlpha = 1.0f - alpha;
        for (int i = 0; i < kFramebufferLen; ++i) {
            const float pr = static_cast<float>(prevFrame_[i].r);
            const float pg = static_cast<float>(prevFrame_[i].g);
            const float pb = static_cast<float>(prevFrame_[i].b);
            const float nr = static_cast<float>(dest[i].r);
            const float ng = static_cast<float>(dest[i].g);
            const float nb = static_cast<float>(dest[i].b);
            const float r = pr * oneMinusAlpha + nr * alpha;
            const float g = pg * oneMinusAlpha + ng * alpha;
            const float b = pb * oneMinusAlpha + nb * alpha;
            dest[i].r = static_cast<uint8_t>(r);
            dest[i].g = static_cast<uint8_t>(g);
            dest[i].b = static_cast<uint8_t>(b);
            prevFrame_[i] = dest[i];
        }
    }

    /// @brief Zero the prev-frame buffer. Call on mode change or boot.
    inline void reset() {
        for (int i = 0; i < kFramebufferLen; ++i) {
            prevFrame_[i].r = 0;
            prevFrame_[i].g = 0;
            prevFrame_[i].b = 0;
        }
    }

private:
    /// Static prev-frame buffer — 320 × 3 bytes = 960 bytes per instance
    /// (struct member; no heap, no PSRAM call). Per-instance lifetime is
    /// owned by the caller.
    CRGB prevFrame_[kFramebufferLen];
    /// Default to mid-softness ≈ 8 Hz cutoff so a freshly constructed
    /// instance behaves sensibly before a configuration call.
    float cutoffHz_ = 8.0f;
};

}  // namespace persistence
}  // namespace effects
}  // namespace lightwaveos
