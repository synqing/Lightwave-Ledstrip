/**
 * @file K1TactusResolver.h
 * @brief K1-Lightwave Stage 3: Tactus Resolver with Family Scoring
 *
 * Resolves the perceptual beat level using family scoring (primary + half + double)
 * and a tactus prior centered at 120 BPM.
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

class K1TactusResolver {
public:
    void begin(uint32_t now_ms);
    void reset();

    /**
     * @brief Resolve tactus from resonator candidates
     * @param in Input resonator frame
     * @param out Output tactus frame
     */
    void updateFromResonators(const K1ResonatorFrame& in, K1TactusFrame& out);

    // Access to state
    float lockedBpm() const { return locked_bpm_; }
    bool  isLocked() const { return locked_; }

private:
    float tactusPrior(float bpm) const;
    int findFamilyMember(const K1ResonatorFrame& in, float target_bpm, float tolerance) const;
    float scoreFamily(const K1ResonatorFrame& in, int candidate_idx) const;

    // Density memory helpers
    void  densityDecay_();
    void  densityAddCandidates_(const K1ResonatorFrame& in);
    int   densityPeakBin_() const;
    float densityPeakStrength01_(int peakBin) const;
    float densityAtBpm_(float bpm) const;

private:
    float locked_bpm_ = 0.0f;
    float locked_phase_ = 0.0f;
    float locked_confidence_ = 0.0f;
    bool  locked_ = false;
    int   locked_bin_ = -1;

    // Hysteresis state
    float challenger_bpm_ = 0.0f;
    int   challenger_frames_ = 0;
    float challenger_score_ = 0.0f;

    // Tempo Density bins
    float density_[ST2_BPM_BINS];

    // Timing
    uint32_t last_update_ms_ = 0;
    uint32_t updates_ = 0;
};

} // namespace k1
} // namespace audio
} // namespace lightwaveos
