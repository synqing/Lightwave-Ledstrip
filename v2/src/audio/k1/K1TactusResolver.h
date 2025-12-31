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

enum class LockState {
    UNLOCKED,   // No tempo detected yet
    PENDING,    // Initial lock, still verifying
    VERIFIED    // Committed lock, full hysteresis active
};

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
    // Grouped density confidence constants (ported from Tab5.DSP)
    static constexpr float GROUP_TOLERANCE = 3.0f;   // BPM range for "same group"
    static constexpr float DISTANT_MIN = 6.0f;       // Minimum separation for "different"

    float tactusPrior(float bpm) const;
    int findFamilyMember(const K1ResonatorFrame& in, float target_bpm, float tolerance) const;
    float scoreFamily(const K1ResonatorFrame& in, int candidate_idx) const;

    /**
     * @brief Compute density confidence using grouped algorithm (from Tab5.DSP)
     *
     * Groups candidates within ±3 BPM of winner as consensus vs distant competitors.
     * If no distant competitor exists, returns 1.0 (unanimous agreement).
     */
    float computeGroupedDensityConf_(const float scores[],
                                      const K1ResonatorFrame& in,
                                      int best_idx) const;

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

    // Lock state machine (Phase 5: pending verification)
    LockState lock_state_ = LockState::UNLOCKED;
    uint32_t lock_pending_start_ms_ = 0;
    float pending_competitor_bpm_ = 0.0f;
    float pending_competitor_score_ = 0.0f;
    int pending_competitor_frames_ = 0;

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
