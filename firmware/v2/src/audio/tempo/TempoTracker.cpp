/**
 * @file TempoTracker.cpp
 * @brief Implementation of Goertzel-based tempo tracker
 *
 * Features:
 * - Adaptive block sizes, interleaved computation, hybrid novelty+VU
 * - VU derivative separation for sustained notes
 * - No window function (novelty already smoothed)
 * - Quartic scaling, silent bin suppression, novelty decay
 *
 * Key features:
 * - Novelty decay (0.999 multiplier per frame) prevents ghost beats from old events
 * - Silence detection suppresses false beats during quiet periods
 *
 * @author LightwaveOS Team
 * @version 1.0.0
 */

#include "TempoTracker.h"
#include <cstring>
#if FEATURE_VALIDATION_PROFILING
#include "../../core/system/ValidationProfiler.h"
#endif

#ifndef NATIVE_BUILD
#if FEATURE_VALIDATION_PROFILING
extern "C" {
#include <esp_timer.h>
}
#endif
#endif

namespace lightwaveos {
namespace audio {

// ============================================================================
// Initialization
// ============================================================================

void TempoTracker::init() {
    // Initialize Goertzel constants for each tempo bin
    for (uint16_t i = 0; i < NUM_TEMPI; i++) {
        float bpm = TEMPO_LOW + static_cast<float>(i);
        float hz = bpm / 60.0f;

        tempi_[i].target_bpm = bpm;
        tempi_[i].target_hz = hz;

        // Phase velocity: radians per frame at reference FPS
        tempi_[i].phase_radians_per_frame = (2.0f * static_cast<float>(M_PI) * hz) / REFERENCE_FPS;

        float neighbor_left_hz;
        float neighbor_right_hz;
        if (i == 0) {
            neighbor_left_hz = (TEMPO_LOW + 0.0f) / 60.0f;
            neighbor_right_hz = (TEMPO_LOW + 1.0f) / 60.0f;
        } else if (i == NUM_TEMPI - 1) {
            neighbor_left_hz = (TEMPO_LOW + static_cast<float>(NUM_TEMPI - 2)) / 60.0f;
            neighbor_right_hz = (TEMPO_LOW + static_cast<float>(NUM_TEMPI - 1)) / 60.0f;
        } else {
            neighbor_left_hz = (TEMPO_LOW + static_cast<float>(i - 1)) / 60.0f;
            neighbor_right_hz = (TEMPO_LOW + static_cast<float>(i + 1)) / 60.0f;
        }

        float dl = fabsf(neighbor_left_hz - hz);
        float dr = fabsf(neighbor_right_hz - hz);
        float max_dist_hz = (dl > dr) ? dl : dr;
        if (max_dist_hz < 1e-6f) max_dist_hz = 1e-6f;

        uint32_t block = static_cast<uint32_t>(SPECTRAL_LOG_HZ / (max_dist_hz * 0.5f));

        // Clamp to valid range
        block = std::min(block, static_cast<uint32_t>(SPECTRAL_HISTORY_LENGTH));
        block = std::max(block, static_cast<uint32_t>(32));
        tempi_[i].block_size = block;

        float w = (2.0f * static_cast<float>(M_PI) * hz) / SPECTRAL_LOG_HZ;
        tempi_[i].cosine = cosf(w);
        tempi_[i].sine = sinf(w);
        tempi_[i].coeff = 2.0f * tempi_[i].cosine;

        // Initialize state
        tempi_[i].phase = 0.0f;
        tempi_[i].phase_inverted = false;
        tempi_[i].magnitude = 0.0f;
        tempi_[i].magnitude_raw = 0.0f;
        tempi_[i].beat = 0.0f;
    }

    // Clear history buffers
    memset(spectral_curve_, 0, sizeof(spectral_curve_));
    memset(vu_curve_, 0, sizeof(vu_curve_));
    memset(tempi_smooth_, 0, sizeof(tempi_smooth_));
    memset(bands_last_, 0, sizeof(bands_last_));

    // Initialize state variables
    spectral_index_ = 0;
    vu_index_ = 0;
    rms_last_ = 0.0f;
    vu_accum_ = 0.0f;
    vu_accum_count_ = 0;
    novelty_scale_ = 1.0f;
    vu_scale_ = 1.0f;
    spectral_scale_count_ = 0;
    vu_scale_count_ = 0;
    calc_bin_ = 0;

    // Start at center of BPM range (~96 BPM)
    // Ensure initialization value is valid (should be 48 for NUM_TEMPI=96)
    static_assert(NUM_TEMPI >= 2, "NUM_TEMPI must be at least 2");
    winner_bin_ = NUM_TEMPI / 2;
    // Validate initialization value
    if (winner_bin_ >= NUM_TEMPI) {
        winner_bin_ = 0;  // Fallback to first bin if calculation is wrong
    }
    candidate_bin_ = winner_bin_;
    if (candidate_bin_ >= NUM_TEMPI) {
        candidate_bin_ = 0;
    }
    candidate_frames_ = 0;
    power_sum_ = 0.0f;
    confidence_ = 0.0f;

    // Output state
    current_phase_ = 0.0f;
    beat_tick_ = false;
    last_tick_ms_ = 0;
    time_ms_ = 0;
}

// ============================================================================
// Novelty Update (dual-rate: VU every hop, spectral when bands ready)
// ============================================================================

void TempoTracker::updateNovelty(const float* bands, uint8_t num_bands,
                                     float rms, bool bands_ready) {
    // ========================================================================
    // VU Derivative (62.5 Hz - every call)
    // Captures all transients between spectral updates
    // ========================================================================
    float vu_delta = std::max(0.0f, rms - rms_last_);
    rms_last_ = rms;

    // Square for dynamic range
    vu_delta *= vu_delta;

    // Apply decay to VU history BEFORE writing new value
    // This ensures new value is not decayed on the same frame
    for (uint16_t i = 0; i < VU_HISTORY_LENGTH; i++) {
        vu_curve_[i] *= NOVELTY_DECAY;
    }

    // Log to VU circular buffer
    vu_curve_[vu_index_] = vu_delta;
    vu_index_ = (vu_index_ + 1) % VU_HISTORY_LENGTH;
    vu_accum_ += vu_delta;
    if (vu_accum_count_ < 255) vu_accum_count_++;

    // ========================================================================
    // Spectral Flux (31.25 Hz - only when bands ready)
    // Provides frequency-weighted beat detection
    // ========================================================================
    if (bands_ready && bands != nullptr && num_bands >= 8) {
        // Perceptual band weights (bass emphasis for beat detection)
        static const float WEIGHTS[8] = {1.4f, 1.3f, 1.0f, 0.9f, 0.8f, 0.6f, 0.4f, 0.3f};
        static const float WEIGHT_SUM = 6.7f;

        float spectral_flux = 0.0f;
        for (uint8_t i = 0; i < 8; i++) {
            float delta = bands[i] - bands_last_[i];
            if (delta > 0.0f) {  // Half-wave rectification (onset only)
                spectral_flux += delta * WEIGHTS[i];
            }
            bands_last_[i] = bands[i];
        }

        // Normalize by weight sum
        spectral_flux /= WEIGHT_SUM;

        // Square for dynamic range
        spectral_flux *= spectral_flux;

        // Apply decay to spectral history BEFORE writing new value
        // This ensures new value is not decayed on the same frame
        for (uint16_t i = 0; i < SPECTRAL_HISTORY_LENGTH; i++) {
            spectral_curve_[i] *= NOVELTY_DECAY;
        }

        // Log spectral flux directly to spectral curve (true VU separation)
        // VU curve is stored separately and combined in computeMagnitude()
        spectral_curve_[spectral_index_] = spectral_flux;
        spectral_index_ = (spectral_index_ + 1) % SPECTRAL_HISTORY_LENGTH;
        vu_accum_ = 0.0f;
        vu_accum_count_ = 0;

        // Update spectral scale when spectral data updates (31.25 Hz)
        // Use counter to update every ~3 spectral updates (~100ms)
        spectral_scale_count_++;
        if (spectral_scale_count_ >= 3) {
            normalizeBuffer(spectral_curve_, nullptr,
                            novelty_scale_, 0.3f, SPECTRAL_HISTORY_LENGTH);
            spectral_scale_count_ = 0;
        }

        // Check for silence to prevent false tracking
        checkSilence();
    }

    // ========================================================================
    // Dynamic Normalization for VU (62.5 Hz - every call)
    // ========================================================================
    // Update VU scale when VU data updates (faster adaptation for transient detection)
    vu_scale_count_++;
    if (vu_scale_count_ >= 6) {  // Every ~6 VU updates (~100ms at 62.5 Hz)
        normalizeBuffer(vu_curve_, nullptr,
                        vu_scale_, 0.3f, VU_HISTORY_LENGTH);
        vu_scale_count_ = 0;
    }
}

void TempoTracker::checkSilence() {
    float min_val = 1.0f;
    float max_val = 0.0f;

    // Look at last 128 samples (~4 sec at 31.25 Hz)
    for (uint16_t i = 0; i < 128; i++) {
        uint16_t idx = (SPECTRAL_HISTORY_LENGTH + spectral_index_ - 128 + i)
                       % SPECTRAL_HISTORY_LENGTH;
        
        // Apply current scale factor to match computeMagnitude() normalization
        float scaled = spectral_curve_[idx] * novelty_scale_;
        scaled = std::min(1.0f, std::max(0.0f, scaled));
        
        // Process as in original (clamp to 0.5, scale, sqrt)
        float processed = std::min(0.5f, scaled) * 2.0f;
        float sqrt_val = sqrtf(processed);
        max_val = std::max(max_val, sqrt_val);
        min_val = std::min(min_val, sqrt_val);
    }

    float contrast = fabsf(max_val - min_val);
    float silence_raw = 1.0f - contrast;

    if (silence_raw > tuning_.silenceThreshold) {
        silence_detected_ = true;
        // Normalize silence level based on threshold
        float range = 1.0f - tuning_.silenceThreshold;
        if (range < 0.001f) range = 0.001f;
        silence_level_ = std::max(0.0f, silence_raw - tuning_.silenceThreshold) / range;
    } else {
        silence_detected_ = false;
        silence_level_ = 0.0f;
    }
}

void TempoTracker::normalizeBuffer(float* buffer, float* normalized,
                                       float& scale, float tau, uint16_t length) {
    // Find maximum in buffer
    float max_val = 0.0f;
    for (uint16_t i = 0; i < length; i++) {
        max_val = std::max(max_val, buffer[i]);
    }

    max_val = std::max(1e-10f, max_val);
    float target_scale = 1.0f / (max_val * 0.5f);
    scale = scale * (1.0f - tau) + target_scale * tau;

    if (normalized) {
        for (uint16_t i = 0; i < length; i++) {
            normalized[i] = buffer[i] * scale;
        }
    }
}

// ============================================================================
// Goertzel Magnitude Computation
// ============================================================================

float TempoTracker::computeMagnitude(uint16_t bin) {
    uint32_t block_size = tempi_[bin].block_size;

    // Clamp to spectral history length
    if (block_size > SPECTRAL_HISTORY_LENGTH) {
        block_size = SPECTRAL_HISTORY_LENGTH;
    }

    float q1 = 0.0f;
    float q2 = 0.0f;

    for (uint32_t i = 0; i < block_size; i++) {
        // Index into spectral circular buffer (oldest to newest)
        uint16_t spectral_idx = (SPECTRAL_HISTORY_LENGTH + spectral_index_ - block_size + i)
                                 % SPECTRAL_HISTORY_LENGTH;

        // Index into VU circular buffer (time-aligned with spectral)
        // VU updates at 62.5 Hz (2x spectral rate), so map time-aligned position
        // Spectral sample i corresponds to time T = i / 31.25 seconds
        // VU sample j corresponds to time T = j / 62.5 seconds
        // For same time T: j = i * 2 (VU has 2 samples per spectral sample)
        // Clamp to available VU history (VU buffer covers half the time window of spectral)
        uint32_t vu_offset = i * 2;
        uint32_t max_vu_offset = VU_HISTORY_LENGTH - 1;
        if (vu_offset > max_vu_offset) {
            vu_offset = max_vu_offset;  // Clamp to available VU history
        }
        uint16_t vu_idx = (VU_HISTORY_LENGTH + vu_index_ - vu_offset)
                          % VU_HISTORY_LENGTH;

        // True VU separation: combine normalized curves
        // Spectral: frequency-weighted flux (31.25 Hz)
        // VU: transient detection (62.5 Hz)
        float spectral_normalized = spectral_curve_[spectral_idx] * novelty_scale_;
        float vu_normalized = vu_curve_[vu_idx] * vu_scale_;

        // Clamp to valid range
        spectral_normalized = std::min(1.0f, std::max(0.0f, spectral_normalized));
        vu_normalized = std::min(1.0f, std::max(0.0f, vu_normalized));

        // Hybrid input: 50% spectral (transients) + 50% VU (sustained)
        float sample = 0.5f * spectral_normalized + 0.5f * vu_normalized;

        // Goertzel IIR step: q0 = coeff * q1 - q2 + sample
        // Window removed - novelty already smoothed
        float q0 = tempi_[bin].coeff * q1 - q2 + sample;
        q2 = q1;
        q1 = q0;
    }

    // Extract real and imaginary components
    float real = q1 - q2 * tempi_[bin].cosine;
    float imag = q2 * tempi_[bin].sine;

    // Extract phase with beat shift compensation
    tempi_[bin].phase = atan2f(imag, real) +
                        (static_cast<float>(M_PI) * BEAT_SHIFT_PERCENT);

    if (tempi_[bin].phase > static_cast<float>(M_PI)) {
        tempi_[bin].phase -= 2.0f * static_cast<float>(M_PI);
        tempi_[bin].phase_inverted = !tempi_[bin].phase_inverted;
    } else if (tempi_[bin].phase < -static_cast<float>(M_PI)) {
        tempi_[bin].phase += 2.0f * static_cast<float>(M_PI);
        tempi_[bin].phase_inverted = !tempi_[bin].phase_inverted;
    }

    // Compute magnitude squared: |H|^2 = q1^2 + q2^2 - q1*q2*coeff
    float mag_sq = q1 * q1 + q2 * q2 - q1 * q2 * tempi_[bin].coeff;

    // Return normalized magnitude
    return sqrtf(std::max(0.0f, mag_sq)) / (static_cast<float>(block_size) / 2.0f);
}

// ============================================================================
// Tempo Update (called per frame, spreads computation)
// ============================================================================

void TempoTracker::updateTempo(float delta_sec) {
    // Interleaved computation: 2 bins per frame
    // At 100 FPS, full spectrum computed every ~48 frames (~0.5s)
    // Compute two bins
    uint16_t bin0 = calc_bin_;
    uint16_t bin1 = (calc_bin_ + 1) % NUM_TEMPI;
    tempi_[bin0].magnitude_raw = computeMagnitude(bin0);
    tempi_[bin1].magnitude_raw = computeMagnitude(bin1);
    calc_bin_ = (calc_bin_ + 2) % NUM_TEMPI;

    // Two-pass auto-ranging
    // Find maximum raw magnitude for normalization
    float max_val = 0.01f;  // Floor to prevent division by zero
    for (uint16_t i = 0; i < NUM_TEMPI; i++) {
        max_val = std::max(max_val, tempi_[i].magnitude_raw);
    }

    float autoranger = 1.0f / max_val;
    power_sum_ = 0.00000001f;  // Small floor to prevent division by zero

    for (uint16_t i = 0; i < NUM_TEMPI; i++) {
        float scaled = tempi_[i].magnitude_raw * autoranger;

        // Quartic scaling for winner exaggeration
        // Makes dominant tempo stand out more clearly
        scaled = scaled * scaled;
        scaled = scaled * scaled;
        tempi_[i].magnitude = scaled;

        // Silent bin suppression
        // Only process bins with significant magnitude
        if (tempi_[i].magnitude > 0.005f) {
            // Active bin: smooth and add to power sum
            tempi_smooth_[i] = tempi_smooth_[i] * 0.975f +
                               tempi_[i].magnitude * 0.025f;
            power_sum_ += tempi_smooth_[i];

            // Sync phase for active bins
            float phase_push = tempi_[i].phase_radians_per_frame * delta_sec * REFERENCE_FPS;
            tempi_[i].phase += phase_push;

            // Wrap phase to [-pi, +pi]
            while (tempi_[i].phase > static_cast<float>(M_PI)) {
                tempi_[i].phase -= 2.0f * static_cast<float>(M_PI);
                tempi_[i].phase_inverted = !tempi_[i].phase_inverted;
            }
            while (tempi_[i].phase < -static_cast<float>(M_PI)) {
                tempi_[i].phase += 2.0f * static_cast<float>(M_PI);
                tempi_[i].phase_inverted = !tempi_[i].phase_inverted;
            }
        } else {
            // Silent bin: decay only, don't add to power sum
            tempi_smooth_[i] *= 0.995f;
        }

        // Beat signal for visualization
        tempi_[i].beat = sinf(tempi_[i].phase);
    }

    // Confidence calculation
    // Find maximum smoothed magnitude
    float max_contribution = 0.00000001f;
    for (uint16_t i = 0; i < NUM_TEMPI; i++) {
        max_contribution = std::max(max_contribution, tempi_smooth_[i]);
    }

    // Confidence = max smoothed / power sum
    if (power_sum_ > 0.01f) {
        confidence_ = max_contribution / power_sum_;
    } else {
        confidence_ = 0.0f;
    }

    // Update winner selection
    updateWinner();

    // Validate winner_bin_ before access (defensive check)
    uint16_t safe_winner = validateWinnerBin();
    if ((safe_winner == bin0 || safe_winner == bin1) && tempi_[safe_winner].magnitude_raw > 0.005f) {
        current_phase_ = tempi_[safe_winner].phase;
    }
}

// ============================================================================
// Bounds Validation
// ============================================================================

/**
 * @brief Validate and clamp winner_bin_ to safe range [0, NUM_TEMPI-1]
 * 
 * DEFENSIVE CHECK: Prevents LoadProhibited crashes from corrupted winner_bin_ index.
 * 
 * If winner_bin_ is corrupted (e.g., by memory corruption, race condition, or
 * uninitialized state), accessing tempi_[winner_bin_] would cause an out-of-bounds
 * memory access and crash the system.
 * 
 * This validation ensures we always access valid array indices, returning a safe
 * default (centre of BPM range) if corruption is detected.
 * 
 * @return Valid bin index, clamped to [0, NUM_TEMPI-1]
 */
uint16_t TempoTracker::validateWinnerBin() const {
#if FEATURE_VALIDATION_PROFILING
#ifndef NATIVE_BUILD
    int64_t start = esp_timer_get_time();
#else
    int64_t start = 0;
#endif
#endif
    if (winner_bin_ >= NUM_TEMPI) {
#if FEATURE_VALIDATION_PROFILING
#ifndef NATIVE_BUILD
        lightwaveos::core::system::ValidationProfiler::recordCall("validateWinnerBin", 
                                                                     esp_timer_get_time() - start);
#else
        lightwaveos::core::system::ValidationProfiler::recordCall("validateWinnerBin", 0);
#endif
#endif
        // Corrupted - reset to safe default (centre of BPM range)
        return NUM_TEMPI / 2;
    }
#if FEATURE_VALIDATION_PROFILING
#ifndef NATIVE_BUILD
    lightwaveos::core::system::ValidationProfiler::recordCall("validateWinnerBin", 
                                                               esp_timer_get_time() - start);
#else
    lightwaveos::core::system::ValidationProfiler::recordCall("validateWinnerBin", 0);
#endif
#endif
    return winner_bin_;
}

// ============================================================================
// Winner Selection with Hysteresis
// ============================================================================

void TempoTracker::updateWinner() {
    // Validate current winner_bin_ before use (defensive check)
    if (winner_bin_ >= NUM_TEMPI) {
        winner_bin_ = NUM_TEMPI / 2;  // Reset to safe default
        candidate_bin_ = winner_bin_;
        candidate_frames_ = 0;
    }
    
    // Find bin with highest smoothed magnitude
    uint16_t best_bin = 0;
    float best_mag = 0.0f;

    for (uint16_t i = 0; i < NUM_TEMPI; i++) {
        if (tempi_smooth_[i] > best_mag) {
            best_mag = tempi_smooth_[i];
            best_bin = i;
        }
    }

    // Ensure best_bin is valid (should always be, but defensive)
    if (best_bin >= NUM_TEMPI) {
        best_bin = NUM_TEMPI / 2;
    }

    // Hysteresis: require advantage for consecutive frames
    if (best_bin != winner_bin_) {
        float current_mag = tempi_smooth_[winner_bin_];

        // Check if challenger has significant advantage
        if (best_mag > current_mag * tuning_.hysteresisThreshold) {
            // Track consecutive frames with this candidate
            if (best_bin == candidate_bin_) {
                candidate_frames_++;
                if (candidate_frames_ >= tuning_.hysteresisFrames) {
                    // Switch to new winner - ensure it's valid
                    winner_bin_ = best_bin;
                    if (winner_bin_ >= NUM_TEMPI) {
                        winner_bin_ = NUM_TEMPI / 2;
                    }
                    candidate_frames_ = 0;
                }
            } else {
                // New candidate, reset counter
                candidate_bin_ = best_bin;
                if (candidate_bin_ >= NUM_TEMPI) {
                    candidate_bin_ = NUM_TEMPI / 2;
                }
                candidate_frames_ = 1;
            }
        } else {
            // Challenger doesn't have enough advantage
            candidate_frames_ = 0;
        }
    } else {
        // Current winner is still strongest
        candidate_frames_ = 0;
    }
    
    // Final validation: ensure winner_bin_ is always valid after update
    if (winner_bin_ >= NUM_TEMPI) {
        winner_bin_ = NUM_TEMPI / 2;
    }
}

// ============================================================================
// Phase Advancement
// ============================================================================

void TempoTracker::advancePhase(float delta_sec) {
    // Validate winner_bin_ to prevent out-of-bounds access
    uint16_t safe_bin = validateWinnerBin();
    
    // Store previous phase for zero-crossing detection
    float last_phase = current_phase_;

    uint32_t step_ms = static_cast<uint32_t>(delta_sec * 1000.0f + 0.5f);
    time_ms_ += step_ms;

    // Advance phase at winner's rate
    // phase_radians_per_frame is calibrated for REFERENCE_FPS
    current_phase_ += tempi_[safe_bin].phase_radians_per_frame *
                      delta_sec * REFERENCE_FPS;

    // Wrap to [-pi, +pi]
    while (current_phase_ > static_cast<float>(M_PI)) {
        current_phase_ -= 2.0f * static_cast<float>(M_PI);
    }
    while (current_phase_ < -static_cast<float>(M_PI)) {
        current_phase_ += 2.0f * static_cast<float>(M_PI);
    }

    // Beat tick detection: zero crossing from negative to positive
    beat_tick_ = (last_phase < 0.0f && current_phase_ >= 0.0f);

    // Debounce: prevent multiple ticks within 60% of beat period
    if (beat_tick_) {
        uint32_t now = time_ms_;
        float beat_period_ms = 60000.0f / tempi_[safe_bin].target_bpm;

        if (now - last_tick_ms_ < static_cast<uint32_t>(beat_period_ms * 0.6f)) {
            beat_tick_ = false;  // Too soon, suppress
        } else {
            last_tick_ms_ = now;
        }
    }
}

// ============================================================================
// Output Accessor
// ============================================================================

TempoOutput TempoTracker::getOutput() const {
    // Validate winner_bin_ to prevent out-of-bounds access
    uint16_t safe_bin = validateWinnerBin();
    
    float conf = confidence_;

    // Suppress confidence during silence
    if (silence_detected_) {
        conf *= (1.0f - silence_level_);
    }

    return TempoOutput{
        .bpm = tempi_[safe_bin].target_bpm,
        .phase01 = (current_phase_ + static_cast<float>(M_PI)) /
                   (2.0f * static_cast<float>(M_PI)),
        .confidence = conf,
        .beat_tick = beat_tick_ && !silence_detected_,
        .locked = conf > 0.3f && !silence_detected_,
        .beat_strength = tempi_smooth_[safe_bin]
    };
}

} // namespace audio
} // namespace lightwaveos

