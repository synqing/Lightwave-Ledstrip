/**
 * @file K1BeatClock.h
 * @brief K1-Lightwave Stage 4: Beat Clock (PLL)
 *
 * Produces a stable phase signal using a Phase-Locked Loop that tracks
 * the resonator phase from Stage 2.
 *
 * Ported from Tab5.DSP for ESP32-S3 Lightwave v2 integration.
 */

#pragma once

#include <stdint.h>
#include "K1Config.h"
#include "K1Types.h"

namespace lightwaveos {
namespace audio {
namespace k1 {

class K1BeatClock {
public:
    void begin(uint32_t now_ms);

    /**
     * @brief Update from tactus resolver
     * @param in Input tactus frame
     * @param now_ms Current timestamp
     * @param out Output beat clock state
     */
    void updateFromTactus(const K1TactusFrame& in, uint32_t now_ms, K1BeatClockState& out);

    /**
     * @brief Advance phase (call every frame)
     * @param now_ms Current timestamp
     * @param delta_sec Time since last tick
     * @param out Output beat clock state
     */
    void tick(uint32_t now_ms, float delta_sec, K1BeatClockState& out);

    // Accessors
    float phase01() const { return phase01_; }
    float bpm() const { return bpm_; }
    bool locked() const { return locked_; }
    float confidence() const { return confidence_; }

    // Debug accessors (for K1DebugCli)
    float phaseError() const { return phase_error_; }
    float freqError() const { return freq_error_; }

private:
    float wrapPhase(float p) const;
    float resonatorPhaseToPhase01(float rp) const;

private:
    float phase_rad_ = 0.0f;
    float phase01_ = 0.0f;
    float bpm_ = 120.0f;
    float ref_phase_rad_ = 0.0f;
    float phase_error_ = 0.0f;
    float freq_error_ = 0.0f;
    uint32_t last_beat_ms_ = 0;
    bool     last_tick_ = false;
    uint32_t last_update_ms_ = 0;
    bool locked_ = false;
    float confidence_ = 0.0f;
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos
