/**
 * @file VoiceMusicClassifier.h
 * @brief Heuristic voice-vs-music probability scalar from ControlBusFrame.
 *
 * Phase 4 Move 4.1 substrate per Topology_Reconciliation §5/§6. Render-trivial
 * scalar producer that combines four signals already on the ControlBus into a
 * smoothed `voiceProb` in [0,1]. 1.0 = confident voice, 0.0 = confident music.
 *
 * This is the cheap heuristic substrate; the ML-class kill-list members
 * (PS-10 mood-driven, AUD-25 mood-classifier) are explicitly DEFERRED to
 * Phase 6 INF-11. Move 4.1 ships ONLY the heuristic so downstream consumers
 * (COM-06 Voice/Music Switcher, F3 Liquid Stillness ambient gate) have a
 * substrate to bind against without waiting for ML infrastructure.
 *
 * ── Heuristic ────────────────────────────────────────────────────────────
 *
 * Voice indicators (push voiceProb toward 1):
 *   1. Flat chroma — vocal pitch is monophonic and time-varying, so over the
 *      smoothing window the chroma vector integrates flat. Computed inline
 *      as `1 - peakiness`, where peakiness = (max(chroma) - mean(chroma)).
 *   2. STM temporal energy in the syllabic 4-8 Hz band (AUD-08 stmTemporal
 *      mel bands). Speech has characteristic syllabic-rate modulation.
 *   3. No tempo lock — voice rarely produces a stable BPM tracker lock.
 *   4. High timbral novelty — formant transitions create spectral churn.
 *
 * Music indicators (push voiceProb toward 0):
 *   1. Peaked chroma (single dominant pitch class) — held chord notes.
 *   2. Tempo lock + high tempoConfidence — periodic beat structure.
 *   3. Low STM temporal energy and low novelty — steady chord progression.
 *
 * The four indicators are combined into a per-frame target probability in
 * [0,1] and then smoothed via dt-correct EMA with τ = 2.0 s to prevent
 * flicker on borderline content.
 *
 * ── Field-availability gap ───────────────────────────────────────────────
 *
 * The original Move 4.1 spec called for spectral centroid (AUD-12) as a
 * voice-band gate (200-4000 Hz) and explicit chroma_strength as a scalar.
 * Neither is currently published on ControlBusFrame:
 *
 *   - AUD-12 spectralCentroid:   NOT on bus. Track via AUD-12 itself.
 *   - chroma_strength scalar:    NOT on bus. Computed inline from chroma[].
 *
 * When AUD-12 lands, extend `update()` to gate the voice indicators by
 * spectral-centroid-in-band rather than raw chroma flatness alone. The
 * current heuristic is adequate for COM-06 substrate and F3 ambient gating
 * but would benefit from centroid for clean-music vs spoken-word edge cases.
 *
 * ── Constraints ──────────────────────────────────────────────────────────
 *
 *  - NO heap allocation. Header-only inline class with built-in scalars.
 *  - dt-correct. EMA alpha computed via `1 - exp(-dt / tau)`.
 *  - Cheap. One pass over chroma[12], a handful of FMAs, one expf per call.
 *    Well under 5 μs typical; trivially within the 2.0 ms render ceiling.
 *  - Render-safe. Safe to call transitively from render() — though the
 *    expected call site is RendererActor pre-frame setup, not inside an
 *    effect's render() loop.
 *
 * ── British English ──────────────────────────────────────────────────────
 * Comments use British spelling (centre, colour, behaviour). Public symbol
 * names follow the existing camelCase + Pascal convention used elsewhere in
 * `src/effects/`.
 */

#pragma once

#include <cmath>
#include <cstdint>

#include "audio/contracts/ControlBus.h"

namespace lightwaveos {
namespace effects {
namespace audio {

class VoiceMusicClassifier {
public:
    /**
     * @brief Update the classifier from the latest ControlBusFrame.
     *
     * Reads chroma peakiness, STM temporal energy, tempo lock, harmonic and
     * timbral novelty from `bus`. Computes a per-frame target probability,
     * then EMA-blends it into the smoothed estimate over `dt` seconds.
     *
     * Silence behaviour: when `bus.audioConfidence` falls below the silence
     * threshold (0.05), the smoothed estimate is held — no drift toward
     * either extreme on dropouts.
     *
     * @param bus  Latest published frame from RendererActor's shared copy.
     * @param dt   Elapsed seconds since the previous update (frame interval).
     */
    void update(const ::lightwaveos::audio::ControlBusFrame& bus, float dt) {
        // Silence freeze: hold prior estimate when no music is present.
        if (bus.audioConfidence < kSilenceThreshold) {
            return;
        }
        if (dt <= 0.0f) {
            return;  // defensive: no-op on non-positive dt
        }

        // ── Indicator 1: chroma flatness ──────────────────────────────────
        // peakiness = max - mean, ∈ [0, 1] for normalised chroma.
        // flatness = 1 - peakiness; high when no single pitch dominates.
        float maxChroma = 0.0f;
        float sumChroma = 0.0f;
        for (uint8_t i = 0; i < ::lightwaveos::audio::CONTROLBUS_NUM_CHROMA; ++i) {
            const float c = bus.chroma[i];
            sumChroma += c;
            if (c > maxChroma) maxChroma = c;
        }
        const float meanChroma =
            sumChroma / static_cast<float>(::lightwaveos::audio::CONTROLBUS_NUM_CHROMA);
        const float peakiness = maxChroma - meanChroma;  // in [0, ~1]
        const float chromaFlat = clamp01(1.0f - peakiness);

        // ── Indicator 2: STM syllabic-rate energy ─────────────────────────
        // stmTemporalEnergy is already a [0,1] mean across mel bands. It
        // peaks for syllabic-rate (4-8 Hz) modulation typical of speech.
        // Only trust it when stmReady (warmup complete).
        const float stmEnergy = bus.stmReady ? clamp01(bus.stmTemporalEnergy) : 0.0f;

        // ── Indicator 3: absence of tempo lock ───────────────────────────
        // Voice rarely yields a stable BPM lock with high confidence.
        // 1.0 = no lock, 0.0 = locked with full confidence.
        const float noTempoLock = bus.tempoLocked
            ? clamp01(1.0f - bus.tempoConfidence)
            : 1.0f;

        // ── Indicator 4: timbral novelty ─────────────────────────────────
        // High formant churn → high timbral novelty. Speech > sustained chord.
        const float timbralChurn = clamp01(bus.saliency.timbralNoveltySmooth);

        // ── Weighted combination ─────────────────────────────────────────
        // Weights chosen so a perfectly-flat-chroma + saturated-STM +
        // no-lock + max-novelty input yields target = 1.0, and a peaked
        // chroma + locked tempo + zero STM + zero novelty yields 0.0.
        constexpr float kW_chroma  = 0.35f;
        constexpr float kW_stm     = 0.30f;
        constexpr float kW_noLock  = 0.20f;
        constexpr float kW_timbre  = 0.15f;
        static_assert(kW_chroma + kW_stm + kW_noLock + kW_timbre > 0.999f &&
                      kW_chroma + kW_stm + kW_noLock + kW_timbre < 1.001f,
                      "VoiceMusicClassifier weights must sum to 1.0");

        const float target = clamp01(
            kW_chroma * chromaFlat +
            kW_stm    * stmEnergy +
            kW_noLock * noTempoLock +
            kW_timbre * timbralChurn);

        // ── dt-correct EMA blend ─────────────────────────────────────────
        // alpha = 1 - exp(-dt / tau). At tau = 2.0 s and dt = 1/60 s,
        // alpha ≈ 0.00831 — slow enough to avoid flicker, fast enough to
        // settle in a few seconds when content shifts.
        const float alpha = 1.0f - expf(-dt / kTau);
        voiceProb_ += (target - voiceProb_) * alpha;

        // Defensive clamp — guards against fp drift over long sessions.
        voiceProb_ = clamp01(voiceProb_);
    }

    /**
     * @brief Voice probability ∈ [0, 1].
     *
     * 1.0 = confident voice, 0.0 = confident music. Default 0.0 (cold start
     * presumed music until evidence accumulates).
     */
    float voiceProb() const { return voiceProb_; }

    /**
     * @brief Reset smoothed state to default (0.0).
     *
     * Call when the audio source changes (mode swap, new stream) so the EMA
     * does not retain history from the previous source.
     */
    void reset() {
        voiceProb_ = 0.0f;
    }

private:
    static constexpr float kTau = 2.0f;             ///< EMA time constant (s)
    static constexpr float kSilenceThreshold = 0.05f;  ///< Below this audioConfidence, freeze.

    static inline float clamp01(float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }

    float voiceProb_ = 0.0f;  ///< Smoothed estimate; seeded to "music" (0.0).
};

}  // namespace audio
}  // namespace effects
}  // namespace lightwaveos
