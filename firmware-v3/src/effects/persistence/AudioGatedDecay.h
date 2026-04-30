/**
 * @file AudioGatedDecay.h
 * @brief Silence-aware exponential decay helper.
 *
 * Phase 4 Move 4.2 PER-18 substrate. Provides a decay primitive whose time
 * constant varies with instantaneous audio RMS:
 *
 *   - RMS at or below `silentRms` → tau = silentTau (fast decay; clears stale
 *     sparkle and trails when the room goes quiet)
 *   - RMS at or above `loudRms`   → tau = loudTau   (slow decay; preserves
 *     persistent trails and ambient afterglow while music plays)
 *   - In between → linear interpolation of tau across the RMS range
 *
 * This is the canonical substrate for the V1.0 F3 "Liquid Stillness" ambient
 * state — effects that bloom during loud passages and dissolve back to black
 * during silence without manual mode switching.
 *
 * The transition is intentionally smooth (linear, no Schmitt hysteresis): the
 * intent is a continuous gradient between "music is here, hold the trail" and
 * "music is gone, fade out", not a discrete state machine.
 *
 * ── Constraints ──────────────────────────────────────────────────────────
 *
 *  - NO heap allocation. The class holds four floats; all helpers operate on
 *    caller-owned storage and are safe to call transitively from render().
 *  - dt-correct semantics. Decay alpha is computed via `1 - exp(-dt / tau)`
 *    so behaviour stays identical at 60 Hz, 120 Hz, or with variable pacing.
 *  - Cheap. Per-frame cost is one `expf` plus an n-element loop; well under
 *    the 2.0 ms render ceiling for n ≤ 320.
 *  - Decay-only. `applyDecay` does NOT take a source value — it implements
 *    the "fade what's there" half of the EMA, leaving the "track this signal"
 *    half to existing helpers (`PersistenceHelpers::emaArrayDt`). This split
 *    matches the F3 Liquid Stillness use case where the audio reactor writes
 *    sparse impulses and the decay layer holds them as trails.
 *
 * ── British English ──────────────────────────────────────────────────────
 * Comments use British spelling (centre, colour, behaviour). Public API names
 * mirror existing PersistenceHelpers.h conventions.
 */

#pragma once

#include <cmath>
#include <cstddef>

#include <FastLED.h>

namespace lightwaveos {
namespace effects {
namespace persistence {

/**
 * @brief Silence-aware exponential decay with RMS-driven time constant.
 *
 * Default configuration is sensible for the canonical ESV11 ControlBus.rms
 * range (roughly 0.0..1.0 normalised):
 *   - silentRms = 0.05  (room noise floor)
 *   - loudRms   = 0.5   (typical music peak in a domestic listening setting)
 *   - silentTau = 0.1 s (fast clear of stale sparkle when silence returns)
 *   - loudTau   = 2.0 s (long ambient trail while music plays)
 *
 * Re-configure via `setRmsRange` / `setTauRange` when calibrating against a
 * different audio backend or aesthetic.
 */
class AudioGatedDecay {
public:
    /**
     * @brief Configure the RMS interval over which tau interpolates.
     *
     * Both endpoints are stored verbatim — no ordering enforcement. If the
     * caller passes silentRms > loudRms, `currentTau` will still produce a
     * monotonic mapping (silence at the high end), which is occasionally
     * useful for inverse-gated behaviour. Out-of-range RMS is clamped at the
     * endpoints regardless of ordering.
     */
    void setRmsRange(float silentRms, float loudRms) {
        silentRms_ = silentRms;
        loudRms_   = loudRms;
    }

    /**
     * @brief Configure the tau interval (seconds) corresponding to the RMS
     * endpoints.
     *
     * Both endpoints must be strictly positive — a zero or negative tau would
     * produce an infinite or undefined alpha. Callers are responsible for
     * supplying sensible values; no clamping is performed.
     */
    void setTauRange(float silentTau_s, float loudTau_s) {
        silentTau_ = silentTau_s;
        loudTau_   = loudTau_s;
    }

    /**
     * @brief Compute the current tau (seconds) for a given RMS reading.
     *
     * Linear interpolation between `silentTau` (at `silentRms`) and `loudTau`
     * (at `loudRms`), clamped at the endpoints. RMS values outside the
     * `[silentRms, loudRms]` interval saturate at the corresponding tau —
     * this prevents pathologically fast or slow decay from out-of-band
     * readings (negative RMS from upstream bugs, or RMS > 1 from a hot
     * gain stage).
     */
    float currentTau(float rms) const {
        // Degenerate range: collapse to silentTau to avoid divide-by-zero.
        const float span = loudRms_ - silentRms_;
        if (span == 0.0f) {
            return silentTau_;
        }
        // Normalise RMS into [0, 1] across the configured interval.
        float t = (rms - silentRms_) / span;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return silentTau_ + t * (loudTau_ - silentTau_);
    }

    /**
     * @brief Apply silence-aware EMA decay to a CRGB array in-place.
     *
     * Computes alpha = 1 - exp(-dt / currentTau(rms)) once, then applies
     * `dest[i] = dest[i] * (1 - alpha)` to every channel of every pixel.
     * No source value is mixed in — this is the "fade what's there" half of
     * the EMA, intended to be paired with sparse impulse writes from an
     * audio-reactive layer.
     *
     * Edge cases:
     *   - `dest == nullptr` or `n == 0`: no-op
     *   - very large tau vs dt: alpha → 0, array unchanged (long trail)
     *   - very small tau vs dt: alpha → 1, array decays toward zero quickly
     */
    void applyDecay(CRGB* dest, size_t n, float rms, float dt) const {
        if (dest == nullptr || n == 0) {
            return;
        }
        const float tau = currentTau(rms);
        const float alpha = 1.0f - expf(-dt / tau);
        const float keep = 1.0f - alpha;
        for (size_t i = 0; i < n; ++i) {
            dest[i].r = static_cast<uint8_t>(static_cast<float>(dest[i].r) * keep);
            dest[i].g = static_cast<uint8_t>(static_cast<float>(dest[i].g) * keep);
            dest[i].b = static_cast<uint8_t>(static_cast<float>(dest[i].b) * keep);
        }
    }

    /**
     * @brief Apply silence-aware EMA decay to a float array in-place.
     *
     * Same contract as `applyDecay(CRGB*)` but for scalar buffers (energy
     * envelopes, VU history, persistence accumulators).
     */
    void applyDecayArray(float* arr, size_t n, float rms, float dt) const {
        if (arr == nullptr || n == 0) {
            return;
        }
        const float tau = currentTau(rms);
        const float alpha = 1.0f - expf(-dt / tau);
        const float keep = 1.0f - alpha;
        for (size_t i = 0; i < n; ++i) {
            arr[i] = arr[i] * keep;
        }
    }

private:
    float silentRms_ = 0.05f;
    float loudRms_   = 0.5f;
    float silentTau_ = 0.1f;
    float loudTau_   = 2.0f;
};

}  // namespace persistence
}  // namespace effects
}  // namespace lightwaveos
