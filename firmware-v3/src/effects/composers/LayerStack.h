/**
 * @file LayerStack.h
 * @brief Overlap-permitted N-buffer composer (sibling-class to ZoneComposer).
 *
 * Phase 1 Move 1.4 (INF-01) per Topology_Reconciliation §5. ZoneComposer
 * partitions the strip into 1..3 disjoint zones; LayerStack permits OVERLAP
 * across the FULL strip, compositing N back-to-front layers via per-layer
 * blend mode and per-layer alpha (`τ` exposure for the INF-02 mandatory
 * pass).
 *
 * The composer is intentionally geometry-agnostic — callers fill each layer
 * buffer with whatever centre-origin (LED 79/80 outward) content they like;
 * LayerStack only stacks them. Layer 0 is always treated as the BASE
 * (OVERWRITE semantics regardless of declared mode), with `alpha` applied
 * as a global brightness scale on its contribution.
 *
 * ── Constraints ──────────────────────────────────────────────────────────
 *
 *  - NO heap allocation. Layer storage is a static C-array member;
 *    `compositeInto`, `writeLayer`, and `setLayer` perform zero
 *    `new`/`malloc`/`String`. Safe to call transitively from render().
 *  - O(N × kStripLen) work per composite. At N=3, kStripLen=320 the budget
 *    is well under 500 µs on ESP32-S3 (≪ 2.0 ms render ceiling).
 *  - dt-agnostic. `alpha` is an INSTANTANEOUS lerp coefficient; time
 *    evolution lives in the caller (which owns the source buffers).
 *  - Centre origin is the caller's contract; LayerStack treats every index
 *    identically.
 *  - Portable. Builds against `<FastLED.h>` on ESP32-S3 and the
 *    `fastled_mock.h` CRGB shim under `NATIVE_BUILD`.
 *
 * ── Reuse ────────────────────────────────────────────────────────────────
 * `BlendMode` is reused verbatim from `effects/zones/BlendMode.h` so blend
 * vocabulary stays unified across the project. We only consume the four
 * modes specified by the move (OVERWRITE / ADDITIVE / ALPHA / MULTIPLY)
 * here — additional modes from BlendMode.h fall through to OVERWRITE.
 *
 * ── British English ──────────────────────────────────────────────────────
 * Comments use British spelling (centre, colour, behaviour, initialise).
 */

#pragma once

#include <FastLED.h>
#include <cstddef>
#include <cstdint>

#include "../zones/BlendMode.h"

namespace lightwaveos {
namespace effects {
namespace composers {

// Bring the canonical BlendMode into the composers namespace under the
// LayerBlendMode alias requested by the Move 1.4 spec. No new enum is
// declared — vocabulary is a strict subset of zones::BlendMode.
using LayerBlendMode = ::lightwaveos::zones::BlendMode;

/**
 * @brief Stack of up to kMaxLayers identically-sized CRGB buffers,
 *        composited back-to-front into a destination strip buffer.
 */
template<size_t kMaxLayers = 3, size_t kStripLen = 320>
class LayerStack {
public:
    LayerStack() {
        // Initialise to a known-clean state — zero buffers, OVERWRITE mode,
        // alpha=0, layerCount_=0 so a default-constructed LayerStack
        // composites to all-black (test 1).
        for (size_t l = 0; l < kMaxLayers; ++l) {
            modes_[l] = LayerBlendMode::OVERWRITE;
            alphas_[l] = 0.0f;
            for (size_t i = 0; i < kStripLen; ++i) {
                layers_[l][i] = CRGB(0, 0, 0);
            }
        }
        layerCount_ = 0;
    }

    /**
     * @brief Configure layer `idx`'s blend mode and alpha.
     *
     * `alpha` is clamped to [0, 1]. Out-of-range `idx` is a defensive
     * no-op (test 7). Configuring layer N implicitly extends the active
     * layer count to `max(layerCount_, idx + 1)` so callers don't need
     * a separate "set count" knob.
     */
    void setLayer(size_t idx, LayerBlendMode mode, float alpha) {
        if (idx >= kMaxLayers) {
            return;  // defensive — silent no-op
        }
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;
        modes_[idx] = mode;
        alphas_[idx] = alpha;
        if (idx + 1 > layerCount_) {
            layerCount_ = idx + 1;
        }
    }

    /**
     * @brief Copy `kStripLen` pixels from `src` into layer `idx`'s buffer.
     *
     * Out-of-range `idx` or null `src` is a defensive no-op. Caller owns
     * the source buffer; LayerStack copies (no aliasing).
     */
    void writeLayer(size_t idx, const CRGB* src) {
        if (idx >= kMaxLayers || src == nullptr) {
            return;
        }
        for (size_t i = 0; i < kStripLen; ++i) {
            layers_[idx][i] = src[i];
        }
    }

    /**
     * @brief Composite all configured layers into `dest`.
     *
     * Layer 0 is the BASE — its declared mode is ignored and OVERWRITE
     * semantics apply, with `alpha` acting as a brightness scale on its
     * contribution (so `alpha=0` on layer 0 yields all-black).
     *
     * Layers 1..layerCount_-1 are then stacked back-to-front using their
     * declared mode and per-layer alpha. The four modes the move exercises
     * are implemented inline so the composer remains independent of
     * FastLED `qadd8`/`scale8` (those are unavailable in the native mock);
     * every other BlendMode value falls through to OVERWRITE.
     *
     * `dest` MUST point to at least `kStripLen` pixels; null is a
     * defensive no-op.
     */
    void compositeInto(CRGB* dest) const {
        if (dest == nullptr) {
            return;
        }

        // Default state — no layers configured: yield all-black so callers
        // don't see uninitialised contents.
        if (layerCount_ == 0) {
            for (size_t i = 0; i < kStripLen; ++i) {
                dest[i] = CRGB(0, 0, 0);
            }
            return;
        }

        // ── Layer 0: BASE ────────────────────────────────────────────────
        // OVERWRITE semantics regardless of declared mode; alpha scales
        // the channel values directly (alpha=0 → black, alpha=1 → verbatim).
        const float a0 = alphas_[0];
        for (size_t i = 0; i < kStripLen; ++i) {
            const CRGB& s = layers_[0][i];
            dest[i].r = scaleByAlpha(s.r, a0);
            dest[i].g = scaleByAlpha(s.g, a0);
            dest[i].b = scaleByAlpha(s.b, a0);
        }

        // ── Layers 1..N-1: declared mode + per-layer alpha ───────────────
        for (size_t l = 1; l < layerCount_; ++l) {
            const LayerBlendMode mode = modes_[l];
            const float a = alphas_[l];

            switch (mode) {
                case LayerBlendMode::ADDITIVE:
                    blendAdditive(dest, layers_[l], a);
                    break;
                case LayerBlendMode::ALPHA:
                    blendAlpha(dest, layers_[l], a);
                    break;
                case LayerBlendMode::MULTIPLY:
                    blendMultiply(dest, layers_[l], a);
                    break;
                case LayerBlendMode::OVERWRITE:
                default:
                    blendOverwrite(dest, layers_[l], a);
                    break;
            }
        }
    }

    /// Active layer count (0..kMaxLayers).
    size_t layerCount() const { return layerCount_; }

private:
    // ── Pixel helpers ─────────────────────────────────────────────────────
    //
    // All blend operators are inline and operate on uint8_t channels in
    // float intermediates so they compile cleanly against both real
    // FastLED and the native CRGB mock. No `qadd8`/`scale8` from FastLED
    // is invoked — keeps the test path FastLED-mock-friendly without
    // diverging from the production semantics.

    static inline uint8_t scaleByAlpha(uint8_t channel, float alpha) {
        const float v = static_cast<float>(channel) * alpha;
        if (v <= 0.0f) return 0;
        if (v >= 255.0f) return 255;
        return static_cast<uint8_t>(v);
    }

    static inline uint8_t saturatingAdd(uint8_t a, uint8_t b) {
        const int sum = static_cast<int>(a) + static_cast<int>(b);
        return (sum > 255) ? 255 : static_cast<uint8_t>(sum);
    }

    // OVERWRITE with per-layer alpha: dest = layer * alpha (alpha=0 erases
    // the layer's contribution, alpha=1 replaces dest verbatim).
    static inline void blendOverwrite(CRGB* dest, const CRGB* layer, float alpha) {
        if (alpha <= 0.0f) return;  // erase: leave dest untouched
        for (size_t i = 0; i < kStripLen; ++i) {
            dest[i].r = scaleByAlpha(layer[i].r, alpha);
            dest[i].g = scaleByAlpha(layer[i].g, alpha);
            dest[i].b = scaleByAlpha(layer[i].b, alpha);
        }
    }

    // ADDITIVE: dest = sat(dest + layer * alpha). alpha=0 ⇒ no contribution
    // (test 6 — per-layer alpha=0 erases). alpha=1 ⇒ full saturating add.
    static inline void blendAdditive(CRGB* dest, const CRGB* layer, float alpha) {
        if (alpha <= 0.0f) return;
        for (size_t i = 0; i < kStripLen; ++i) {
            const uint8_t lr = scaleByAlpha(layer[i].r, alpha);
            const uint8_t lg = scaleByAlpha(layer[i].g, alpha);
            const uint8_t lb = scaleByAlpha(layer[i].b, alpha);
            dest[i].r = saturatingAdd(dest[i].r, lr);
            dest[i].g = saturatingAdd(dest[i].g, lg);
            dest[i].b = saturatingAdd(dest[i].b, lb);
        }
    }

    // ALPHA: dest = lerp(dest, layer, alpha). The per-layer alpha is the
    // lerp coefficient itself — distinct from BlendMode.h's hard-coded
    // 50/50 ALPHA (which lacks a tunable knob).
    static inline void blendAlpha(CRGB* dest, const CRGB* layer, float alpha) {
        if (alpha <= 0.0f) return;
        const float oneMinusAlpha = 1.0f - alpha;
        for (size_t i = 0; i < kStripLen; ++i) {
            const float r = static_cast<float>(dest[i].r) * oneMinusAlpha +
                            static_cast<float>(layer[i].r) * alpha;
            const float g = static_cast<float>(dest[i].g) * oneMinusAlpha +
                            static_cast<float>(layer[i].g) * alpha;
            const float b = static_cast<float>(dest[i].b) * oneMinusAlpha +
                            static_cast<float>(layer[i].b) * alpha;
            dest[i].r = (r >= 255.0f) ? 255 : (r <= 0.0f ? 0 : static_cast<uint8_t>(r));
            dest[i].g = (g >= 255.0f) ? 255 : (g <= 0.0f ? 0 : static_cast<uint8_t>(g));
            dest[i].b = (b >= 255.0f) ? 255 : (b <= 0.0f ? 0 : static_cast<uint8_t>(b));
        }
    }

    // MULTIPLY: dest = (dest * layer) / 255. Per-layer alpha lerps between
    // the unmultiplied dest (alpha=0, no effect) and the fully-multiplied
    // result (alpha=1) so a per-layer alpha=0 still erases the layer.
    static inline void blendMultiply(CRGB* dest, const CRGB* layer, float alpha) {
        if (alpha <= 0.0f) return;
        const float oneMinusAlpha = 1.0f - alpha;
        for (size_t i = 0; i < kStripLen; ++i) {
            const uint8_t mr = static_cast<uint8_t>(
                (static_cast<int>(dest[i].r) * static_cast<int>(layer[i].r)) / 255);
            const uint8_t mg = static_cast<uint8_t>(
                (static_cast<int>(dest[i].g) * static_cast<int>(layer[i].g)) / 255);
            const uint8_t mb = static_cast<uint8_t>(
                (static_cast<int>(dest[i].b) * static_cast<int>(layer[i].b)) / 255);
            const float r = static_cast<float>(dest[i].r) * oneMinusAlpha +
                            static_cast<float>(mr) * alpha;
            const float g = static_cast<float>(dest[i].g) * oneMinusAlpha +
                            static_cast<float>(mg) * alpha;
            const float b = static_cast<float>(dest[i].b) * oneMinusAlpha +
                            static_cast<float>(mb) * alpha;
            dest[i].r = (r >= 255.0f) ? 255 : (r <= 0.0f ? 0 : static_cast<uint8_t>(r));
            dest[i].g = (g >= 255.0f) ? 255 : (g <= 0.0f ? 0 : static_cast<uint8_t>(g));
            dest[i].b = (b >= 255.0f) ? 255 : (b <= 0.0f ? 0 : static_cast<uint8_t>(b));
        }
    }

    // ── Storage ───────────────────────────────────────────────────────────
    // Static C-array — no heap. At kMaxLayers=3, kStripLen=320:
    //   3 × 320 × sizeof(CRGB) = 2880 bytes per LayerStack instance.
    CRGB           layers_[kMaxLayers][kStripLen];
    LayerBlendMode modes_[kMaxLayers];
    float          alphas_[kMaxLayers];
    size_t         layerCount_ = 0;
};

}  // namespace composers
}  // namespace effects
}  // namespace lightwaveos
