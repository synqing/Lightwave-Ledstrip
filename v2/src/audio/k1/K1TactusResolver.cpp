/**
 * @file K1TactusResolver.cpp
 * @brief K1-Lightwave Stage 3: Tactus Resolver Implementation
 *
 * Ported from Tab5.DSP for ESP32-S3 Lightwave v2 integration.
 */

#include "K1TactusResolver.h"
#include <cmath>
#if K1_DEBUG_TACTUS_SCORES
#include <Arduino.h>
#endif

namespace lightwaveos {
namespace audio {
namespace k1 {

void K1TactusResolver::begin(uint32_t now_ms) {
    last_update_ms_ = now_ms;
    updates_ = 0;
    reset();
}

void K1TactusResolver::reset() {
    locked_bpm_ = 0.0f;
    locked_phase_ = 0.0f;
    locked_confidence_ = 0.0f;
    locked_ = false;
    locked_bin_ = -1;

    lock_state_ = LockState::UNLOCKED;
    lock_pending_start_ms_ = 0;
    pending_competitor_bpm_ = 0.0f;
    pending_competitor_score_ = 0.0f;
    pending_competitor_frames_ = 0;

    challenger_bpm_ = 0.0f;
    challenger_frames_ = 0;
    challenger_score_ = 0.0f;

    for (int i = 0; i < ST2_BPM_BINS; i++) {
        density_[i] = ST3D_DENSITY_FLOOR;
    }
}

float K1TactusResolver::densityAtBpm_(float bpm) const {
    int bin = (int)lroundf(bpm - (float)ST2_BPM_MIN);
    if (bin < 0) bin = 0;
    if (bin >= ST2_BPM_BINS) bin = ST2_BPM_BINS - 1;
    return density_[bin];
}

void K1TactusResolver::densityDecay_() {
#if ST3D_ENABLE
    for (int i = 0; i < ST2_BPM_BINS; i++) {
        density_[i] *= ST3D_DECAY;
        if (density_[i] < ST3D_DENSITY_FLOOR) density_[i] = ST3D_DENSITY_FLOOR;
    }
#endif
}

void K1TactusResolver::densityAddCandidates_(const K1ResonatorFrame& in) {
#if ST3D_ENABLE
    int useK = in.k;
    if (useK > ST3D_TOPK_USE) useK = ST3D_TOPK_USE;

    for (int i = 0; i < useK; i++) {
        float bpm = in.candidates[i].bpm;
        float mag = in.candidates[i].magnitude;

        if (!K1_IS_VALID_FLOAT(bpm) || !K1_IS_VALID_FLOAT(mag)) continue;
        if (mag < ST3D_MIN_ADD_MAG) continue;

        int centerBin = (int)lroundf(bpm - (float)ST2_BPM_MIN);
        if (centerBin < 0 || centerBin >= ST2_BPM_BINS) continue;

        for (int dx = -ST3D_KERNEL_RADIUS_BPM; dx <= ST3D_KERNEL_RADIUS_BPM; dx++) {
            int b = centerBin + dx;
            if (b < 0 || b >= ST2_BPM_BINS) continue;

#if ST3D_KERNEL_SHAPE_TRI
            float w = 1.0f - (k1_absf((float)dx) / (float)(ST3D_KERNEL_RADIUS_BPM + 1));
#else
            float w = 1.0f;
#endif
            density_[b] += mag * w;
        }
    }
#endif
}

int K1TactusResolver::densityPeakBin_() const {
    int best = 0;
    float bestV = density_[0];
    for (int i = 1; i < ST2_BPM_BINS; i++) {
        if (density_[i] > bestV) {
            bestV = density_[i];
            best = i;
        }
    }
    return best;
}

float K1TactusResolver::densityPeakStrength01_(int peakBin) const {
    float peak = density_[peakBin];
    float ru = ST3D_DENSITY_FLOOR;
    for (int i = 0; i < ST2_BPM_BINS; i++) {
        if (i == peakBin) continue;
        if (density_[i] > ru) ru = density_[i];
    }
    float denom = peak + ru;
    if (denom <= 0.0f) return 0.0f;
    float r = peak / denom;
    float out = (r - 0.5f) * 2.0f;
    return k1_clamp01(out);
}

float K1TactusResolver::tactusPrior(float bpm) const {
    float diff = bpm - ST3_TACTUS_CENTER;
    return expf(-(diff * diff) / (2.0f * ST3_TACTUS_SIGMA * ST3_TACTUS_SIGMA));
}

int K1TactusResolver::findFamilyMember(const K1ResonatorFrame& in,
                                        float target_bpm,
                                        float tolerance) const {
    int best_idx = -1;
    float best_dist = tolerance;

    for (int i = 0; i < in.k; ++i) {
        float dist = fabsf(in.candidates[i].bpm - target_bpm);
        if (dist < best_dist && in.candidates[i].magnitude > 0.05f) {
            best_dist = dist;
            best_idx = i;
        }
    }

    return best_idx;
}

// ============================================================================
// Grouped Density Confidence (ported from Tab5.DSP stage3_tactus.cpp:341-399)
// ============================================================================
float K1TactusResolver::computeGroupedDensityConf_(
    const float scores[],
    const K1ResonatorFrame& in,
    int best_idx) const
{
    if (best_idx < 0 || best_idx >= in.k) return 0.0f;

    float best_bpm = in.candidates[best_idx].bpm;
    float group_score = 0.0f;

    // Sum all scores within GROUP_TOLERANCE of winner
    // These are "the same tempo" with slightly different estimates
    for (int i = 0; i < in.k; ++i) {
        float dist = fabsf(in.candidates[i].bpm - best_bpm);
        if (dist <= GROUP_TOLERANCE) {
            group_score += scores[i];
        }
    }

    // Find best DISTANT runner-up (must be far away to be a genuine competitor)
    float distant_runner = 0.0f;
    for (int i = 0; i < in.k; ++i) {
        float dist = fabsf(in.candidates[i].bpm - best_bpm);
        if (dist >= DISTANT_MIN && scores[i] > distant_runner) {
            distant_runner = scores[i];
        }
    }

    // If no distant competitor, unanimous agreement = high confidence
    // This is the key insight: 64, 65, 66 BPM all agreeing means HIGH confidence
    // not competition between similar estimates
    if (distant_runner < 0.01f) {
        return 1.0f;
    }

    // Symmetric margin: group vs distant
    float denom = group_score + distant_runner;
    if (denom <= 0.0f) return 0.0f;
    return k1_clamp01((group_score - distant_runner) / denom);
}

float K1TactusResolver::scoreFamily(const K1ResonatorFrame& in, int candidate_idx) const {
    if (candidate_idx < 0 || candidate_idx >= in.k) return 0.0f;

    float primary_bpm = in.candidates[candidate_idx].bpm;
    float primary_mag = in.candidates[candidate_idx].magnitude;

    if (primary_bpm < (float)ST2_BPM_MIN || primary_bpm > (float)ST2_BPM_MAX) return 0.0f;

    float score = primary_mag * tactusPrior(primary_bpm);

    // Octave-only family scoring
    float half_bpm = primary_bpm / 2.0f;
    if (half_bpm >= (float)ST2_BPM_MIN) {
        float half_tolerance = half_bpm * 0.03f;
        int half_idx = findFamilyMember(in, half_bpm, half_tolerance);
        if (half_idx >= 0) {
            float half_mag = in.candidates[half_idx].magnitude;
            score += ST3_HALF_CONTRIB * half_mag * tactusPrior(primary_bpm);
        }
    }

    float double_bpm = primary_bpm * 2.0f;
    if (double_bpm <= (float)ST2_BPM_MAX) {
        float double_tolerance = double_bpm * 0.03f;
        int double_idx = findFamilyMember(in, double_bpm, double_tolerance);
        if (double_idx >= 0) {
            float double_mag = in.candidates[double_idx].magnitude;
            score += ST3_DOUBLE_CONTRIB * double_mag * tactusPrior(primary_bpm);
        }
    }

#if ST3D_ENABLE
    int peakBin = densityPeakBin_();
    float peakV = density_[peakBin];
    float d = densityAtBpm_(primary_bpm);
    float dn = (peakV > 0.0f) ? (d / peakV) : 0.0f;
    dn = k1_clamp01(dn);
    score *= (1.0f + 0.80f * dn);
#endif

    // Stability bonus if near current lock (only in VERIFIED state)
    if (locked_ && lock_state_ == LockState::VERIFIED && locked_bpm_ > 0.0f) {
        float dist_to_lock = fabsf(primary_bpm - locked_bpm_);
        if (dist_to_lock < ST3_STABILITY_WINDOW) {
            score += ST3_STABILITY_BONUS;
        }
    }

    return score;
}

void K1TactusResolver::updateFromResonators(const K1ResonatorFrame& in, K1TactusFrame& out) {
    last_update_ms_ = in.t_ms;
    updates_++;

    densityDecay_();
    densityAddCandidates_(in);

    // Score all candidates
    float best_score = 0.0f;
    int best_idx = -1;
    float scores[ST2_TOPK] = {0};

    for (int i = 0; i < in.k; ++i) {
        if (in.candidates[i].bpm < (float)ST2_BPM_MIN) continue;
        if (in.candidates[i].magnitude < 0.1f) continue;

        float score = scoreFamily(in, i);
        scores[i] = score;

        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    // ========================================================================
    // Octave Doubling Override (half-time detection)
    // If best is suspiciously low (< 80 BPM), check if double is more likely
    // ========================================================================
    if (best_idx >= 0) {
        float best_bpm_check = in.candidates[best_idx].bpm;
        if (best_bpm_check < 80.0f) {
            float double_bpm = best_bpm_check * 2.0f;
            if (double_bpm >= 60.0f && double_bpm <= 180.0f) {
                // Find candidate near double
                int double_idx = findFamilyMember(in, double_bpm, 4.0f);
                if (double_idx >= 0) {
                    float double_prior = tactusPrior(double_bpm);
                    float half_prior = tactusPrior(best_bpm_check);

                    // If double has much better prior (closer to 120 BPM), prefer it
                    // Threshold: double's prior must be at least 2x better
                    if (double_prior > half_prior * 2.0f) {
                        float double_score = scores[double_idx];
                        // Double must have reasonable score (at least 30% of best)
                        if (double_score > best_score * 0.3f) {
                            best_idx = double_idx;
                            best_score = double_score;
                        }
                    }
                }
            }
        }
    }

    // Compute density_conf using grouped algorithm (ported from Tab5.DSP)
    // Groups nearby candidates as consensus rather than competition
    float density_conf = computeGroupedDensityConf_(scores, in, best_idx);

#if K1_DEBUG_TACTUS_SCORES
    // Diagnostic: Show top 5 candidates with scores
    Serial.printf("[K1] Candidates: ");
    int show_count = (in.k < 5) ? in.k : 5;
    for (int i = 0; i < show_count; ++i) {
        float raw_mag = in.candidates[i].magnitude;
        float prior = tactusPrior(in.candidates[i].bpm);
        float score = scores[i];
        Serial.printf("%.1f(m=%.2f,p=%.2f,s=%.2f) ",
            in.candidates[i].bpm, raw_mag, prior, score);
    }
    Serial.println();
    if (locked_) {
        Serial.printf("[K1] Lock: %.1f StabBonus: %s\n",
            locked_bpm_,
            (best_idx >= 0 && fabsf(in.candidates[best_idx].bpm - locked_bpm_) < ST3_STABILITY_WINDOW) ? "YES" : "NO");
    }
#endif

    // No valid candidate
    if (best_idx < 0 || best_score < ST3_MIN_CONFIDENCE) {
        out.t_ms = in.t_ms;
        out.bpm = locked_bpm_;
        // Confidence derived from density_conf (grouped consensus), not clamped family_score
        out.confidence = density_conf * (1.0f - 0.2f * (1.0f - density_conf));  // Slight boost for high agreement
        out.density_conf = density_conf;
        out.phase_hint = locked_phase_;
        out.locked = locked_;
        out.winning_bin = best_idx;
        out.challenger_frames = 0;
        out.family_score = best_score;
        return;
    }

    float best_bpm = in.candidates[best_idx].bpm;
    float best_phase = in.candidates[best_idx].phase;

    // First lock: enter PENDING state instead of immediate commit
    if (lock_state_ == LockState::UNLOCKED) {
        locked_bpm_ = best_bpm;
        locked_phase_ = best_phase;
        // Confidence derived from density_conf, not clamped family_score
        locked_confidence_ = density_conf * (1.0f - 0.2f * (1.0f - density_conf));
        locked_ = true;
        locked_bin_ = best_idx;
        lock_state_ = LockState::PENDING;
        lock_pending_start_ms_ = in.t_ms;
        pending_competitor_bpm_ = 0.0f;
        pending_competitor_score_ = 0.0f;
        pending_competitor_frames_ = 0;

        challenger_bpm_ = 0.0f;
        challenger_frames_ = 0;

        out.t_ms = in.t_ms;
        out.bpm = locked_bpm_;
        out.confidence = locked_confidence_;
        out.density_conf = density_conf;
        out.phase_hint = locked_phase_;
        out.locked = true;
        out.winning_bin = best_idx;
        out.challenger_frames = 0;
        out.family_score = best_score;
        // Don't return - continue to check if best matches lock or handle verification
    }

    // In PENDING state: track strongest competitor and verify
    if (lock_state_ == LockState::PENDING) {
        uint32_t elapsed = in.t_ms - lock_pending_start_ms_;

        // Check for strong competitor (>5 BPM away with >10% advantage)
        if (fabsf(best_bpm - locked_bpm_) > 5.0f &&
            best_score > locked_confidence_ * COMPETITOR_THRESHOLD) {
            if (fabsf(best_bpm - pending_competitor_bpm_) < 3.0f) {
                pending_competitor_frames_++;
                pending_competitor_score_ = best_score;
            } else {
                pending_competitor_bpm_ = best_bpm;
                pending_competitor_frames_ = 1;
                pending_competitor_score_ = best_score;
            }

            // If competitor sustains for 1.5s during verification (15 frames at 10 Hz), switch
            if (pending_competitor_frames_ >= 15) {
                locked_bpm_ = pending_competitor_bpm_;
                locked_phase_ = best_phase;
                locked_confidence_ = density_conf * (1.0f - 0.2f * (1.0f - density_conf));
                locked_bin_ = best_idx;
                pending_competitor_frames_ = 0;
                // Reset verification period
                lock_pending_start_ms_ = in.t_ms;
            }
        } else {
            pending_competitor_frames_ = 0;
        }

        // After verification period, commit to VERIFIED state
        if (elapsed >= LOCK_VERIFY_MS) {
            lock_state_ = LockState::VERIFIED;
        }
    }

    // Check if best is same as current lock (applies to both PENDING and VERIFIED)
    float dist_to_lock = fabsf(best_bpm - locked_bpm_);
    if (dist_to_lock < ST3_STABILITY_WINDOW) {
        // Slow tracking - update lock smoothly
        locked_bpm_ = 0.99f * locked_bpm_ + 0.01f * best_bpm;
        locked_phase_ = best_phase;
        // Confidence derived from density_conf, not clamped family_score
        locked_confidence_ = density_conf * (1.0f - 0.2f * (1.0f - density_conf));
        locked_bin_ = best_idx;

        challenger_bpm_ = 0.0f;
        challenger_frames_ = 0;
        // Reset competitor tracking if we're back to the locked tempo
        pending_competitor_frames_ = 0;

        out.t_ms = in.t_ms;
        out.bpm = locked_bpm_;
        out.confidence = locked_confidence_;
        out.density_conf = density_conf;
        out.phase_hint = locked_phase_;
        out.locked = true;
        out.winning_bin = best_idx;
        out.challenger_frames = 0;
        out.family_score = best_score;
        return;
    }

    // Challenger logic
    float incumbent_score = locked_confidence_;
    if (locked_bin_ >= 0 && locked_bin_ < in.k) {
        int incumbent_idx = findFamilyMember(in, locked_bpm_, ST3_STABILITY_WINDOW);
        if (incumbent_idx >= 0) {
            float saved_bpm = locked_bpm_;
            locked_ = false;
            incumbent_score = scoreFamily(in, incumbent_idx);
            locked_ = true;
            locked_bpm_ = saved_bpm;
        }
    }

    float ratio = best_score / (incumbent_score + 0.001f);

    if (ratio > ST3_SWITCH_RATIO) {
        float bpm_diff = fabsf(best_bpm - challenger_bpm_);
        if (challenger_bpm_ > 0.0f && bpm_diff < ST3_STABILITY_WINDOW) {
            challenger_frames_++;
            challenger_score_ = best_score;
        } else {
            challenger_bpm_ = best_bpm;
            challenger_frames_ = 1;
            challenger_score_ = best_score;
        }

        if (challenger_frames_ >= ST3_SWITCH_FRAMES) {
            locked_bpm_ = best_bpm;
            locked_phase_ = best_phase;
            // Confidence derived from density_conf, not clamped family_score
            locked_confidence_ = density_conf * (1.0f - 0.2f * (1.0f - density_conf));
            locked_bin_ = best_idx;

            challenger_bpm_ = 0.0f;
            challenger_frames_ = 0;
        }
    } else {
        challenger_bpm_ = 0.0f;
        challenger_frames_ = 0;
    }

    out.t_ms = in.t_ms;
    out.bpm = locked_bpm_;
    // Confidence already derived from density_conf, just clamp to [0,1]
    out.confidence = k1_clamp01(locked_confidence_);
    out.density_conf = density_conf;
    out.phase_hint = locked_phase_;
    out.locked = locked_;
    out.winning_bin = locked_bin_;
    out.challenger_frames = challenger_frames_;
    out.family_score = best_score;
}

} // namespace k1
} // namespace audio
} // namespace lightwaveos
