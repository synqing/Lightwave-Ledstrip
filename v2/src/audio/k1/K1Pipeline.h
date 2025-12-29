/**
 * @file K1Pipeline.h
 * @brief K1-Lightwave Beat Tracker Pipeline Orchestrator
 *
 * Orchestrates Stages 2-4 of the K1 beat tracker:
 *   Stage 2: Resonator Bank (121 Goertzel bins)
 *   Stage 3: Tactus Resolver (family scoring + hysteresis)
 *   Stage 4: Beat Clock (PLL phase tracking)
 *
 * Input: Spectral flux from AudioActor, scaled to z-score
 * Output: Beat clock state (phase01, beat_tick, bpm, confidence)
 */

#pragma once

#include <stdint.h>
#include "K1Config.h"
#include "K1Types.h"
#include "K1ResonatorBank.h"
#include "K1TactusResolver.h"
#include "K1BeatClock.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Pipeline output for cross-core communication
 *
 * This struct is designed to be pushed to a lock-free queue
 * for thread-safe consumption by RendererActor.
 */
struct K1PipelineOutput {
    uint32_t t_ms       = 0;
    float phase01       = 0.0f;   // [0, 1) phase, 0 = beat
    bool  beat_tick     = false;  // true for one frame at beat
    float bpm           = 120.0f;
    float confidence    = 0.0f;
    bool  locked        = false;
};

class K1Pipeline {
public:
    /**
     * @brief Initialize the pipeline
     * @param now_ms Current timestamp
     */
    void begin(uint32_t now_ms);

    /**
     * @brief Reset all pipeline state
     */
    void reset();

    /**
     * @brief Process one novelty frame from AudioActor
     *
     * Call this at ~62.5 Hz (every audio hop).
     * The flux value from ControlBusFrame is scaled to z-score internally.
     *
     * @param flux Spectral flux from ControlBusFrame [0, 1]
     * @param t_ms Current timestamp
     * @param out Output (updated every call, beat_tick only true at wrap)
     * @return true if tempo lock changed
     */
    bool processNovelty(float flux, uint32_t t_ms, K1PipelineOutput& out);

    /**
     * @brief Advance phase without new novelty data
     *
     * Call this from RendererActor at render rate (~120 FPS) to keep
     * phase01 advancing smoothly between audio updates.
     *
     * @param now_ms Current timestamp
     * @param delta_sec Time since last call
     * @param out Output with updated phase
     */
    void tick(uint32_t now_ms, float delta_sec, K1PipelineOutput& out);

    // Accessors
    float bpm() const { return beat_clock_.bpm(); }
    float phase01() const { return beat_clock_.phase01(); }
    float confidence() const { return beat_clock_.confidence(); }
    bool  locked() const { return beat_clock_.locked(); }

    // Stage access for debugging
    const K1ResonatorBank& resonators() const { return resonators_; }
    const K1TactusResolver& tactus() const { return tactus_; }
    const K1BeatClock& beatClock() const { return beat_clock_; }

private:
    /**
     * @brief Scale Lightwave flux [0,1] to K1 z-score [-6, +6]
     */
    static float fluxToZScore(float flux);

private:
    K1ResonatorBank resonators_;
    K1TactusResolver tactus_;
    K1BeatClock beat_clock_;

    // Last outputs for delta computation
    K1ResonatorFrame last_resonator_frame_;
    K1TactusFrame last_tactus_frame_;
    K1BeatClockState last_beat_clock_;

    // Timing
    uint32_t last_t_ms_ = 0;
    bool first_frame_ = true;
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos
