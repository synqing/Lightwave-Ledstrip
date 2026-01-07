/**
 * @file TempoTracker.cpp
 * @brief Implementation of onset-timing tempo tracker
 *
 * Architecture (3 layers):
 * - Layer 1: Onset detection (spectral flux + VU derivative)
 * - Layer 2: Beat tracking (inter-onset timing + PLL phase lock)
 * - Layer 3: Output formatting (BeatState → TempoOutput)
 *
 * Reference implementation: z2.md (onset_detector + beat_tracker)
 *
 * @author LightwaveOS Team
 * @version 2.0.0 - Onset-timing rewrite
 */

#include "TempoTracker.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include "../AudioDebugConfig.h"

// #region agent log
// Native-safe debug logging using sample counter (not system timers)
static void debug_log(uint8_t minVerbosity, const char* location, const char* message, const char* data_json, uint64_t t_samples) {
    auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
    if (dbgCfg.verbosity < minVerbosity) {
        return;  // Suppress if verbosity too low
    }
    // Output JSON to serial with special prefix for parsing
    // Convert t_samples to microseconds for logging: t_us = (t_samples * 1000000ULL) / 16000
    uint64_t t_us = (t_samples * 1000000ULL) / 16000;
    printf("DEBUG_JSON:{\"location\":\"%s\",\"message\":\"%s\",\"data\":%s,\"timestamp\":%llu}\n",
           location, message, data_json, (unsigned long long)t_us);
}
// #endregion

namespace lightwaveos {
namespace audio {

// ============================================================================
// Initialization
// ============================================================================

void TempoTracker::init() {
    // Initialize onset detection state
    // Initialize to small but reasonable value (typical flux is 0.01-0.1 range)
    // This prevents huge normalized values on first few frames
    // Ensure minimum floor to prevent decay to zero
    onset_state_.baseline_vu = 0.01f;
    onset_state_.baseline_spec = 0.01f;
    // Safety check: ensure they're at least at minimum floor
    const float MIN_BASELINE_INIT = 0.001f;
    if (onset_state_.baseline_vu < MIN_BASELINE_INIT) onset_state_.baseline_vu = MIN_BASELINE_INIT;
    if (onset_state_.baseline_spec < MIN_BASELINE_INIT) onset_state_.baseline_spec = MIN_BASELINE_INIT;
    onset_state_.flux_prev = 0.0f;
    onset_state_.flux_prevprev = 0.0f;
    onset_state_.lastOnsetUs = 0;  // Store in samples
    onset_state_.rms_last = 0.0f;
    memset(onset_state_.bands_last, 0, sizeof(onset_state_.bands_last));

    // Initialize beat tracking state
    beat_state_.bpm = 120.0f;
    beat_state_.phase01 = 0.0f;
    beat_state_.conf = 0.0f;
    beat_state_.lastUs = 0;
    beat_state_.lastOnsetUs = 0;
    beat_state_.periodSecEma = 0.5f;    // 120 BPM = 0.5 sec period
    beat_state_.periodAlpha = 0.15f;
    beat_state_.correctionCheckCounter = 0;
    beat_state_.lastCorrectionBpm = 120.0f;
    beat_state_.intervalCount = 0;
    memset(beat_state_.recentIntervals, 0, sizeof(beat_state_.recentIntervals));
    
    // Initialize tempo density buffer
    memset(beat_state_.tempoDensity, 0, sizeof(beat_state_.tempoDensity));
    
    // Initialize 2nd-order PLL state
    beat_state_.phaseErrorIntegral = 0.0f;
    
    // Initialize jitter tracking
    beat_state_.bpmHistoryIdx = 0;
    memset(beat_state_.bpmHistory, 0, sizeof(beat_state_.bpmHistory));
    beat_state_.beatTickHistoryIdx = 0;
    memset(beat_state_.beatTickHistory, 0, sizeof(beat_state_.beatTickHistory));
    
    // Initialize octave flip detection
    beat_state_.lastBpmFromDensity = 0.0f;

    // Initialize diagnostics
    last_onset_ = false;
    onset_strength_ = 0.0f;
    combined_flux_ = 0.0f;

    // Initialize diagnostic state
    memset(&diagnostics_, 0, sizeof(diagnostics_));
    diagnostics_.lastOnsetInterval = 0.0f;
    diagnostics_.lastValidInterval = 0.0f;
    diagnostics_.lastRejectedInterval = 0.0f;
    diagnostics_.lastConfidenceDelta = 0.0f;
    diagnostics_.isLocked = false;
    diagnostics_.lockTimeMs = 0;
    diagnostics_.bpmJitter = 0.0f;
    diagnostics_.phaseJitter = 0.0f;
    diagnostics_.octaveFlips = 0;
    
    // Record initialization time
    // Use sample counter timebase (native-safe, deterministic)
    m_initTime = 0;  // Will be set from first update call

    // Initialize output state
    beat_tick_ = false;
    last_phase_ = 0.0f;
    last_tick_samples_ = 0;
    
    // Initialize summary logging
    summaryLogCounter_ = 0;
}

// ============================================================================
// Layer 1: Onset Detection
// ============================================================================

void TempoTracker::updateNovelty(const float* bands, uint8_t num_bands,
                                     float rms, bool bands_ready, uint64_t tMicros) {
    // Track call frequency for hop rate verification (verbosity 5)
    static uint64_t lastUpdateNoveltyUs = 0;
    static uint32_t updateNoveltyCallCount = 0;
    updateNoveltyCallCount++;
    if (lastUpdateNoveltyUs > 0) {
        uint64_t hopIntervalUs = (tMicros > lastUpdateNoveltyUs) ? (tMicros - lastUpdateNoveltyUs) : 0ULL;
        auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
        if (dbgCfg.verbosity >= 5 && (updateNoveltyCallCount % 62 == 0)) {  // Log every ~1 second
            char hop_info[256];
            snprintf(hop_info, sizeof(hop_info),
                "{\"hop_interval_us\":%llu,\"hop_interval_ms\":%.2f,\"call_count\":%u,"
                "\"expected_hop_ms\":16.0,\"tMicros\":%llu}",
                (unsigned long long)hopIntervalUs, hopIntervalUs * 1e-3f, updateNoveltyCallCount,
                (unsigned long long)tMicros);
            debug_log(5, "TempoTracker.cpp:updateNovelty", "hop_rate", hop_info, tMicros);
        }
    }
    lastUpdateNoveltyUs = tMicros;
    // Compute VU derivative (every call)
    float vu_delta = std::max(0.0f, rms - onset_state_.rms_last);
    onset_state_.rms_last = rms;
    // REMOVED: vu_delta *= vu_delta;  // Squaring makes values too small

    // Compute spectral flux (only when bands ready)
    float spectral_flux = 0.0f;
    if (bands_ready && bands != nullptr && num_bands >= 8) {
        // Weight sum for normalization
        float weight_sum = 0.0f;
        for (uint8_t i = 0; i < 8; i++) {
            weight_sum += tuning_.spectralWeights[i];
        }

        // Weighted spectral flux (half-wave rectified)
        for (uint8_t i = 0; i < 8; i++) {
            float delta = bands[i] - onset_state_.bands_last[i];
            if (delta > 0.0f) {  // Onset only (positive deltas)
                spectral_flux += delta * tuning_.spectralWeights[i];
            }
            onset_state_.bands_last[i] = bands[i];
        }

        // Normalize by weight sum
        spectral_flux /= weight_sum;
        // REMOVED: spectral_flux *= spectral_flux;  // Squaring makes values too small
    }

    // Update baselines separately (with peak gating)
    // This must happen before normalization so baselines track raw flux values
    // Minimum baseline floor to prevent decay to zero
    const float MIN_BASELINE_VU = 0.001f;
    const float MIN_BASELINE_SPEC = 0.001f;
    
    float vu_thresh = onset_state_.baseline_vu * tuning_.onsetThreshK;
    float baseline_vu_before = onset_state_.baseline_vu;
    if (vu_delta <= vu_thresh) {
        // Normal update when below threshold
        onset_state_.baseline_vu = (1.0f - tuning_.baselineAlpha) * onset_state_.baseline_vu + 
                                    tuning_.baselineAlpha * vu_delta;
        // Enforce minimum floor
        if (onset_state_.baseline_vu < MIN_BASELINE_VU) {
            onset_state_.baseline_vu = MIN_BASELINE_VU;
        }
    } else {
        // Peak gating: cap contribution to prevent contamination
        // Use max of current baseline or minimum to ensure recovery from near-zero
        float effective_baseline = std::max(onset_state_.baseline_vu, MIN_BASELINE_VU);
        float capped_vu = std::min(vu_delta, effective_baseline * 1.5f);
        onset_state_.baseline_vu = (1.0f - tuning_.baselineAlpha) * onset_state_.baseline_vu + 
                                    tuning_.baselineAlpha * capped_vu;
        // Enforce minimum floor
        if (onset_state_.baseline_vu < MIN_BASELINE_VU) {
            onset_state_.baseline_vu = MIN_BASELINE_VU;
        }
    }
    
    // Update spectral baseline (with peak gating)
    float baseline_spec_before = onset_state_.baseline_spec;
    if (bands_ready) {
        float spec_thresh = onset_state_.baseline_spec * tuning_.onsetThreshK;
        if (spectral_flux <= spec_thresh) {
            onset_state_.baseline_spec = (1.0f - tuning_.baselineAlpha) * onset_state_.baseline_spec + 
                                          tuning_.baselineAlpha * spectral_flux;
            // Enforce minimum floor
            if (onset_state_.baseline_spec < MIN_BASELINE_SPEC) {
                onset_state_.baseline_spec = MIN_BASELINE_SPEC;
            }
        } else {
            // Peak gating: cap contribution to prevent contamination
            // Use max of current baseline or minimum to ensure recovery from near-zero
            float effective_baseline = std::max(onset_state_.baseline_spec, MIN_BASELINE_SPEC);
            float capped_spec = std::min(spectral_flux, effective_baseline * 1.5f);
            onset_state_.baseline_spec = (1.0f - tuning_.baselineAlpha) * onset_state_.baseline_spec + 
                                          tuning_.baselineAlpha * capped_spec;
            // Enforce minimum floor
            if (onset_state_.baseline_spec < MIN_BASELINE_SPEC) {
                onset_state_.baseline_spec = MIN_BASELINE_SPEC;
            }
        }
    }
    
    // Baseline adaptation logging (verbosity 5, periodic)
    static uint32_t baseline_log_counter = 0;
    baseline_log_counter++;
    auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
    if (dbgCfg.verbosity >= 5 && (baseline_log_counter % 62 == 0)) {  // Log every ~1 second
        char baseline_data[512];
        snprintf(baseline_data, sizeof(baseline_data),
            "{\"baseline_vu_before\":%.6f,\"baseline_vu_after\":%.6f,"
            "\"baseline_spec_before\":%.6f,\"baseline_spec_after\":%.6f,"
            "\"vu_delta\":%.6f,\"spectral_flux\":%.6f,\"vu_thresh\":%.6f,"
            "\"spec_thresh\":%.6f,\"baselineAlpha\":%.3f,\"tMicros\":%llu}",
            baseline_vu_before, onset_state_.baseline_vu,
            baseline_spec_before, onset_state_.baseline_spec,
            vu_delta, spectral_flux, vu_thresh,
            bands_ready ? (onset_state_.baseline_spec * tuning_.onsetThreshK) : 0.0f,
            tuning_.baselineAlpha, (unsigned long long)tMicros);
        debug_log(5, "TempoTracker.cpp:updateNovelty", "baseline_adaptation", baseline_data, tMicros);
    }

    // Normalize each stream before combining (scale-invariant)
    const float eps = 1e-6f;
    float vu_n = vu_delta / (onset_state_.baseline_vu + eps);
    float spec_n = (bands_ready && spectral_flux > 0.0f) ? 
                   spectral_flux / (onset_state_.baseline_spec + eps) : 0.0f;
    
    // Clamp normalized values to prevent extreme outliers
    vu_n = std::max(0.0f, std::min(10.0f, vu_n));
    spec_n = std::max(0.0f, std::min(10.0f, spec_n));
    
    // Combine with configurable weights (default 50/50)
    const float w_vu = 0.5f;
    const float w_spec = 0.5f;
    if (bands_ready) {
        combined_flux_ = w_spec * spec_n + w_vu * vu_n;
    } else {
        combined_flux_ = vu_n;  // VU only when bands not ready
    }

    // Detailed flux calculation logging (verbosity 5)
    if (dbgCfg.verbosity >= 5) {
        char flux_calc[512];
        snprintf(flux_calc, sizeof(flux_calc),
            "{\"vu_delta_raw\":%.6f,\"spectral_flux_raw\":%.6f,"
            "\"vu_n\":%.6f,\"spec_n\":%.6f,\"combined_flux\":%.6f,"
            "\"baseline_vu\":%.6f,\"baseline_spec\":%.6f,\"bands_ready\":%d,"
            "\"tMicros\":%llu}",
            vu_delta, spectral_flux,
            vu_n, spec_n, combined_flux_,
            onset_state_.baseline_vu, onset_state_.baseline_spec, bands_ready ? 1 : 0,
            (unsigned long long)tMicros);
        debug_log(5, "TempoTracker.cpp:updateNovelty", "flux_calculation", flux_calc, tMicros);
    }

    float strength = 0.0f;
    // Convert tMicros to t_samples for detectOnset (temporary during migration)
    uint64_t t_samples = (tMicros * 16000ULL) / 1000000ULL;
    last_onset_ = detectOnset(combined_flux_, t_samples, strength);
    onset_strength_ = strength;
}

// ============================================================================
// K1 Feature Consumption
// ============================================================================

void TempoTracker::updateFromFeatures(const k1::AudioFeatureFrame& frame) {
    // Use rhythm_novelty as primary onset evidence (already scale-invariant from K1)
    float novelty = frame.rhythm_novelty;
    
    // Optional: Use rhythm_energy for secondary VU derivative (if needed)
    // For now, we use novelty directly as it's already normalized
    
    // Get timing from sample counter
    uint64_t t_samples = frame.t_samples;
    
    // Set init time on first call
    if (m_initTime == 0) {
        m_initTime = t_samples;
    }
    
    // Update combined flux from novelty (K1 already provides normalized novelty)
    combined_flux_ = novelty;
    
    // #region agent log
    static uint32_t log_counter = 0;
    if ((log_counter++ % 125) == 0) {  // Log every ~1 second
        char novelty_data[256];
        snprintf(novelty_data, sizeof(novelty_data),
            "{\"novelty\":%.6f,\"rhythm_energy\":%.6f,\"t_samples\":%llu,\"hypothesisId\":\"A\"}",
            novelty, frame.rhythm_energy, (unsigned long long)t_samples);
        debug_log(3, "TempoTracker.cpp:updateFromFeatures", "k1_novelty", novelty_data, t_samples);
    }
    // #endregion
    
    // Detect onset from novelty
    float strength = 0.0f;
    bool onset = detectOnset(combined_flux_, t_samples, strength);
    
    // #region agent log
    if (onset && (log_counter % 10 == 0)) {  // Log every 10th onset
        char onset_detected[256];
        snprintf(onset_detected, sizeof(onset_detected),
            "{\"onset\":true,\"strength\":%.6f,\"novelty\":%.6f,\"t_samples\":%llu,\"hypothesisId\":\"A\"}",
            strength, novelty, (unsigned long long)t_samples);
        debug_log(3, "TempoTracker.cpp:updateFromFeatures", "onset_detected", onset_detected, t_samples);
    }
    // #endregion
    
    // Store for beat tracking
    last_onset_ = onset;
    onset_strength_ = strength;
    
    // Update beat tracking
    float delta_sec = 128.0f / 16000.0f;  // Hop duration at 16kHz
    updateBeat(onset, strength, t_samples, delta_sec);
    updateTempo(delta_sec, t_samples);
}

// ============================================================================
// Onset Detection (from z2.md onset_detector.h)
// ============================================================================

bool TempoTracker::detectOnset(float flux, uint64_t t_samples, float& outStrength) {
    // Update peak detection history
    float flux_curr = flux;
    float flux_prev = onset_state_.flux_prev;
    float flux_prevprev = onset_state_.flux_prevprev;
    
    // Compute combined baseline for threshold
    float combined_baseline = (onset_state_.baseline_vu * 0.5f + onset_state_.baseline_spec * 0.5f);
    float thresh = combined_baseline * tuning_.onsetThreshK;
    
    // Check for local peak: prev > prevprev AND prev > curr AND prev > thresh
    bool is_local_peak = (flux_prev > flux_prevprev) && 
                         (flux_prev > flux_curr) && 
                         (flux_prev > thresh);
    
    // #region agent log
    static uint32_t peak_log_counter = 0;
    if ((peak_log_counter++ % 250) == 0) {  // Log every ~2 seconds
        char peak_check[256];
        snprintf(peak_check, sizeof(peak_check),
            "{\"flux_prev\":%.6f,\"flux_prevprev\":%.6f,\"flux_curr\":%.6f,\"thresh\":%.6f,"
            "\"is_local_peak\":%d,\"prev_gt_prevprev\":%d,\"prev_gt_curr\":%d,\"prev_gt_thresh\":%d,\"hypothesisId\":\"A\"}",
            flux_prev, flux_prevprev, flux_curr, thresh,
            is_local_peak ? 1 : 0,
            (flux_prev > flux_prevprev) ? 1 : 0,
            (flux_prev > flux_curr) ? 1 : 0,
            (flux_prev > thresh) ? 1 : 0);
        debug_log(3, "TempoTracker.cpp:detectOnset", "peak_check", peak_check, t_samples);
    }
    // #endregion
    
    // Detailed flux trace logging (verbosity 5)
    auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
    if (dbgCfg.verbosity >= 5) {
        char flux_trace[512];
        snprintf(flux_trace, sizeof(flux_trace),
            "{\"flux_prevprev\":%.6f,\"flux_prev\":%.6f,\"flux_curr\":%.6f,"
            "\"is_local_peak\":%d,\"baseline_vu\":%.6f,\"baseline_spec\":%.6f,"
            "\"combined_baseline\":%.6f,\"threshold\":%.6f,"
            "\"peak_check_prev_gt_prevprev\":%d,\"peak_check_prev_gt_curr\":%d,\"peak_check_prev_gt_thresh\":%d}",
            flux_prevprev, flux_prev, flux_curr,
            is_local_peak ? 1 : 0,
            onset_state_.baseline_vu, onset_state_.baseline_spec,
            combined_baseline, thresh,
            (flux_prev > flux_prevprev) ? 1 : 0,
            (flux_prev > flux_curr) ? 1 : 0,
            (flux_prev > thresh) ? 1 : 0);
        debug_log(5, "TempoTracker.cpp:detectOnset", "flux_trace", flux_trace, t_samples);
    }
    
    // Update history for next call
    onset_state_.flux_prevprev = flux_prev;
    onset_state_.flux_prev = flux_curr;
    
    // Only fire if local peak detected
    if (!is_local_peak) {
        outStrength = 0.0f;
        // Update diagnostics
        diagnostics_.currentFlux = flux;
        diagnostics_.baseline = combined_baseline;
        diagnostics_.threshold = thresh;
        return false;  // Not a peak, reject
    }

    // Refractory period check (convert samples to microseconds)
    uint64_t refrUs = static_cast<uint64_t>(tuning_.refractoryMs) * 1000ULL;
    uint64_t t_us = (t_samples * 1000000ULL) / 16000;
    uint64_t lastOnsetUs = (onset_state_.lastOnsetUs * 1000000ULL) / 16000;
    uint64_t timeSinceLastUs = (t_us > lastOnsetUs) ? (t_us - lastOnsetUs) : 0ULL;
    bool canFire = timeSinceLastUs > refrUs;

    // Update diagnostic tracking
    diagnostics_.currentFlux = flux;
    diagnostics_.baseline = combined_baseline;
    diagnostics_.threshold = thresh;

    // #region agent log
    uint64_t lastOnsetTimeUs = (diagnostics_.lastOnsetTime * 1000000ULL) / 16000;
    float timeSinceLast = (diagnostics_.lastOnsetTime != 0) ? 
        static_cast<float>(t_us - lastOnsetTimeUs) * 1e-6f : 0.0f;
    char onset_data[512];
    snprintf(onset_data, sizeof(onset_data), 
        "{\"flux\":%.6f,\"baseline\":%.6f,\"threshold\":%.6f,\"canFire\":%d,"
        "\"timeSinceLast\":%.3f,\"timeSinceLastUs\":%llu,\"refrUs\":%llu,"
        "\"lastOnsetUs\":%llu,\"t_samples\":%llu,\"hypothesisId\":\"A,B,C\"}",
        flux, combined_baseline, thresh, canFire ? 1 : 0, timeSinceLast,
        (unsigned long long)timeSinceLastUs, (unsigned long long)refrUs,
        (unsigned long long)lastOnsetUs, (unsigned long long)t_samples);
    debug_log(5, "TempoTracker.cpp:150", "onset_check", onset_data, t_samples);
    // #endregion

    if (canFire && flux > thresh) {
        // Calculate inter-onset interval if we have a previous onset
        float interval = 0.0f;
        uint64_t intervalUs = 0ULL;
        if (diagnostics_.lastOnsetTime != 0) {
            uint64_t lastOnsetTimeUs = (diagnostics_.lastOnsetTime * 1000000ULL) / 16000;
            intervalUs = (t_us > lastOnsetTimeUs) ? (t_us - lastOnsetTimeUs) : 0ULL;
            interval = static_cast<float>(intervalUs) * 1e-6f;
            diagnostics_.lastOnsetInterval = interval;
        }
        
        uint64_t oldLastOnsetSamples = onset_state_.lastOnsetUs;
        onset_state_.lastOnsetUs = t_samples;  // Store in samples
        diagnostics_.lastOnsetTime = t_samples;  // Store in samples
        diagnostics_.onsetCount++;
        
        outStrength = (flux - thresh) / (thresh + 1e-6f);
        // Clamp strength to [0, 5]
        outStrength = std::max(0.0f, std::min(5.0f, outStrength));
        
        // #region agent log
        char fired_data[512];
        snprintf(fired_data, sizeof(fired_data), 
            "{\"interval\":%.3f,\"intervalUs\":%llu,\"strength\":%.3f,"
            "\"oldLastOnsetSamples\":%llu,\"newLastOnsetSamples\":%llu,\"t_samples\":%llu,"
            "\"hypothesisId\":\"A\"}",
            interval, (unsigned long long)intervalUs, outStrength,
            (unsigned long long)oldLastOnsetSamples, (unsigned long long)onset_state_.lastOnsetUs,
            (unsigned long long)t_samples);
        debug_log(5, "TempoTracker.cpp:166", "onset_fired", fired_data, t_samples);
        // #endregion
        
        return true;
    }

    // Track rejection reasons
    if (!canFire) {
        diagnostics_.onsetRejectedRefractory++;
        // #region agent log
        char rej_data[128];
        snprintf(rej_data, sizeof(rej_data), "{\"reason\":\"refractory\",\"hypothesisId\":\"E\"}");
        debug_log(5, "TempoTracker.cpp:178", "onset_rejected", rej_data, t_samples);
        // #endregion
    } else if (flux <= thresh) {
        diagnostics_.onsetRejectedThreshold++;
        // #region agent log
        char rej_data[256];
        snprintf(rej_data, sizeof(rej_data), 
            "{\"reason\":\"threshold\",\"flux\":%.6f,\"threshold\":%.6f,\"diff\":%.6f,\"hypothesisId\":\"A,B\"}",
            flux, thresh, thresh - flux);
        debug_log(5, "TempoTracker.cpp:184", "onset_rejected", rej_data, t_samples);
        // #endregion
    }

    outStrength = 0.0f;
    return false;
}

// ============================================================================
// Layer 2: Beat Tracking
// ============================================================================

void TempoTracker::updateTempo(float delta_sec, uint64_t t_samples) {
    // #region agent log
    static uint32_t tempo_log_counter = 0;
    float density_before_decay[BeatState::DENSITY_BINS];
    if ((tempo_log_counter++ % 125) == 0) {  // Log every ~1 second
        // Capture density before decay
        for (int i = 0; i < BeatState::DENSITY_BINS; i++) {
            density_before_decay[i] = beat_state_.tempoDensity[i];
        }
    }
    // #endregion
    
    // Decay density buffer
    for (int i = 0; i < BeatState::DENSITY_BINS; i++) {
        beat_state_.tempoDensity[i] *= beat_state_.densityDecay;
    }
    
    // #region agent log
    if ((tempo_log_counter % 125) == 1) {  // Log right after decay
        float max_before = 0.0f, max_after = 0.0f;
        int peak_before = 0, peak_after = 0;
        for (int i = 0; i < BeatState::DENSITY_BINS; i++) {
            if (density_before_decay[i] > max_before) {
                max_before = density_before_decay[i];
                peak_before = i;
            }
            if (beat_state_.tempoDensity[i] > max_after) {
                max_after = beat_state_.tempoDensity[i];
                peak_after = i;
            }
        }
        char decay_data[256];
        snprintf(decay_data, sizeof(decay_data),
            "{\"decay_factor\":%.3f,\"peak_before\":%d,\"max_before\":%.6f,\"peak_after\":%d,\"max_after\":%.6f,\"hypothesisId\":\"C\"}",
            beat_state_.densityDecay, peak_before, max_before, peak_after, max_after);
        debug_log(3, "TempoTracker.cpp:updateTempo", "density_decay", decay_data, t_samples);
    }
    // #endregion
    
    // Update beat tracking (this will add to density buffer if onset detected)
    updateBeat(last_onset_, onset_strength_, t_samples, delta_sec);
    
    // Find peak bin in density buffer
    float maxDensity = 0.0f;
    int peakBin = 0;
    for (int i = 0; i < BeatState::DENSITY_BINS; i++) {
        if (beat_state_.tempoDensity[i] > maxDensity) {
            maxDensity = beat_state_.tempoDensity[i];
            peakBin = i;
        }
    }
    
    // #region agent log
    if ((tempo_log_counter % 125) == 2) {  // Log after updateBeat
        char density_after_update[256];
        snprintf(density_after_update, sizeof(density_after_update),
            "{\"peak_bin\":%d,\"peak_density\":%.6f,\"bpm_hat\":%.1f,\"hypothesisId\":\"D\"}",
            peakBin, maxDensity, BeatState::DENSITY_MIN_BPM + static_cast<float>(peakBin));
        debug_log(3, "TempoTracker.cpp:updateTempo", "density_after_update", density_after_update, t_samples);
    }
    // #endregion
    
    // Find second peak (for confidence calculation)
    float secondPeak = 0.0f;
    for (int i = 0; i < BeatState::DENSITY_BINS; i++) {
        if (i != peakBin && beat_state_.tempoDensity[i] > secondPeak) {
            secondPeak = beat_state_.tempoDensity[i];
        }
    }
    
    // Estimate BPM from peak
    float bpm_hat = BeatState::DENSITY_MIN_BPM + static_cast<float>(peakBin);
    
    // Compute confidence from peak sharpness
    const float eps = 1e-6f;
    float conf_from_density = (maxDensity - secondPeak) / (maxDensity + eps);
    conf_from_density = std::max(0.0f, std::min(1.0f, conf_from_density));
    
    // Smooth BPM estimate (EMA)
    const float bpmAlpha = 0.1f;  // Slow smoothing
    beat_state_.bpm = (1.0f - bpmAlpha) * beat_state_.bpm + bpmAlpha * bpm_hat;
    
    // Update confidence from density (with temporal smoothing)
    const float confAlpha = 0.2f;
    beat_state_.conf = (1.0f - confAlpha) * beat_state_.conf + confAlpha * conf_from_density;
    
    // Track lock time
    if (beat_state_.conf > 0.5f && !diagnostics_.isLocked) {
        diagnostics_.isLocked = true;
        diagnostics_.lockStartTime = t_samples;
        if (diagnostics_.lockTimeMs == 0) {
            // First lock - record time from init (convert samples to ms)
            diagnostics_.lockTimeMs = ((t_samples - m_initTime) * 1000ULL) / 16000;
        }
    } else if (beat_state_.conf <= 0.5f && diagnostics_.isLocked) {
        diagnostics_.isLocked = false;
    }
    
    // Detect octave flips (large BPM jumps)
    if (beat_state_.lastBpmFromDensity > 0.0f) {
        float ratio = bpm_hat / beat_state_.lastBpmFromDensity;
        if (ratio > 1.8f || ratio < 0.55f) {  // Near 2x or 0.5x
            diagnostics_.octaveFlips++;
        }
    }
    beat_state_.lastBpmFromDensity = bpm_hat;
    
    // Track BPM history for jitter calculation
    if (diagnostics_.isLocked) {
        beat_state_.bpmHistory[beat_state_.bpmHistoryIdx] = beat_state_.bpm;
        beat_state_.bpmHistoryIdx = (beat_state_.bpmHistoryIdx + 1) % 10;
        
        // Compute RMS jitter
        float mean = 0.0f;
        for (int i = 0; i < 10; i++) {
            mean += beat_state_.bpmHistory[i];
        }
        mean /= 10.0f;
        
        float variance = 0.0f;
        for (int i = 0; i < 10; i++) {
            float diff = beat_state_.bpmHistory[i] - mean;
            variance += diff * diff;
        }
        diagnostics_.bpmJitter = std::sqrt(variance / 10.0f);
    }
    
    // Periodic summary log (verbosity >= 3, every ~1 second)
    summaryLogCounter_++;
    if (summaryLogCounter_ >= SUMMARY_LOG_INTERVAL) {
        summaryLogCounter_ = 0;
        
        auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
        if (dbgCfg.verbosity >= 3) {
            char summary_data[512];
            snprintf(summary_data, sizeof(summary_data),
                "{\"bpm\":%.1f,\"bpm_hat\":%.1f,\"conf\":%.2f,\"locked\":%d,"
                "\"density_peak_bin\":%d,\"density_peak_val\":%.2f,\"density_second_peak\":%.2f,"
                "\"onsets_total\":%u,\"onsets_rej_refr\":%u,\"onsets_rej_thr\":%u,"
                "\"intervals_valid\":%u,\"intervals_rej\":%u,\"rejection_rate_pct\":%.1f,"
                "\"last_valid_interval\":%.3f,\"last_valid_bpm\":%.1f,"
                "\"bpm_jitter\":%.2f,\"phase_jitter_ms\":%.1f,\"octave_flips\":%u,"
                "\"lock_time_ms\":%llu}",
                beat_state_.bpm, bpm_hat, beat_state_.conf, diagnostics_.isLocked ? 1 : 0,
                peakBin, maxDensity, secondPeak,
                diagnostics_.onsetCount, diagnostics_.onsetRejectedRefractory, diagnostics_.onsetRejectedThreshold,
                diagnostics_.intervalsValid, diagnostics_.intervalsRejected,
                (diagnostics_.intervalsValid + diagnostics_.intervalsRejected > 0) ?
                    (100.0f * diagnostics_.intervalsRejected / (diagnostics_.intervalsValid + diagnostics_.intervalsRejected)) : 0.0f,
                diagnostics_.lastValidInterval,
                (diagnostics_.lastValidInterval > 0.0f) ? (60.0f / diagnostics_.lastValidInterval) : 0.0f,
                diagnostics_.bpmJitter, diagnostics_.phaseJitter, diagnostics_.octaveFlips,
                diagnostics_.lockTimeMs);
            debug_log(3, "TempoTracker.cpp:updateTempo", "tempo_summary", summary_data, t_samples);
        }
    }
    
    // Log significant BPM changes (verbosity >= 4, only when change > 2 BPM)
    static float lastLoggedBpm = 0.0f;
    if (std::abs(beat_state_.bpm - lastLoggedBpm) > 2.0f) {
        auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
        if (dbgCfg.verbosity >= 4) {
            char bpm_change_data[256];
            snprintf(bpm_change_data, sizeof(bpm_change_data),
                "{\"old_bpm\":%.1f,\"new_bpm\":%.1f,\"bpm_hat\":%.1f,\"conf\":%.2f,\"density_peak\":%d}",
                lastLoggedBpm, beat_state_.bpm, bpm_hat, beat_state_.conf, peakBin);
            debug_log(4, "TempoTracker.cpp:updateTempo", "bpm_change", bpm_change_data, t_samples);
        }
        lastLoggedBpm = beat_state_.bpm;
    }
    
    // Log confidence threshold crossings (verbosity >= 3)
    static float lastLoggedConf = 0.0f;
    bool crossedLock = (lastLoggedConf <= 0.5f && beat_state_.conf > 0.5f) ||
                       (lastLoggedConf > 0.5f && beat_state_.conf <= 0.5f);
    if (crossedLock) {
        auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
        if (dbgCfg.verbosity >= 3) {
            char conf_cross_data[256];
            snprintf(conf_cross_data, sizeof(conf_cross_data),
                "{\"conf\":%.2f,\"locked\":%d,\"bpm\":%.1f,\"lock_time_ms\":%llu}",
                beat_state_.conf, diagnostics_.isLocked ? 1 : 0, beat_state_.bpm, diagnostics_.lockTimeMs);
            debug_log(3, "TempoTracker.cpp:updateTempo", "confidence_threshold", conf_cross_data, t_samples);
        }
        lastLoggedConf = beat_state_.conf;
    }
    
    // Log density buffer peak shifts (verbosity >= 5, only when peak bin changes)
    static int lastPeakBin = -1;
    if (peakBin != lastPeakBin && lastPeakBin >= 0) {
        auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
        if (dbgCfg.verbosity >= 5) {
            char density_shift_data[256];
            snprintf(density_shift_data, sizeof(density_shift_data),
                "{\"old_peak_bin\":%d,\"new_peak_bin\":%d,\"old_bpm\":%.1f,\"new_bpm\":%.1f,\"peak_density\":%.2f}",
                lastPeakBin, peakBin,
                BeatState::DENSITY_MIN_BPM + lastPeakBin,
                bpm_hat, maxDensity);
            debug_log(5, "TempoTracker.cpp:updateTempo", "density_peak_shift", density_shift_data, t_samples);
        }
    }
    lastPeakBin = peakBin;
}

// ============================================================================
// Beat Tracking (from z2.md beat_tracker.h)
// ============================================================================

void TempoTracker::updateBeat(bool onset, float onsetStrength, uint64_t t_samples, float delta_sec) {
    // Initialize timestamp on first call
    if (beat_state_.lastUs == 0) {
        beat_state_.lastUs = t_samples;  // Store in samples
    }

    // Compute time delta (convert samples to seconds)
    float dt = static_cast<float>(t_samples - beat_state_.lastUs) / 16000.0f;
    if (dt < 0.0f) dt = 0.0f;
    beat_state_.lastUs = t_samples;  // Store in samples

    // Advance phase from current BPM
    float period = 60.0f / (beat_state_.bpm + 1e-6f);
    beat_state_.phase01 += dt / period;
    beat_state_.phase01 -= floorf(beat_state_.phase01);

    // Track confidence changes for diagnostics
    float confBefore = beat_state_.conf;

    // Confidence decay over time (if no support)
    beat_state_.conf -= tuning_.confFall * dt;
    if (beat_state_.conf < 0.0f) {
        beat_state_.conf = 0.0f;
    }

    // Track confidence decay
    float confDelta = beat_state_.conf - confBefore;
    if (confDelta < 0.0f) {
        diagnostics_.confidenceFalls++;
        diagnostics_.lastConfidenceDelta = confDelta;
    }

    if (onset) {
        // Beat-candidate gating: only promote onsets to beat-candidates
        // Minimum candidate interval: minDt = 60/(maxBpm*2) ≈ 0.166s at 180 BPM
        const float minDt = 60.0f / (tuning_.maxBpm * 2.0f);  // ≈ 0.166s at 180 BPM
        
        // Estimate beat period from time since last onset
        if (beat_state_.lastOnsetUs != 0) {
            float onsetDt = static_cast<float>(t_samples - beat_state_.lastOnsetUs) / 16000.0f;
            
            // #region agent log
            static uint32_t minDt_log_counter = 0;
            if ((minDt_log_counter++ % 50) == 0) {  // Log every 50th check
                char minDt_check[256];
                snprintf(minDt_check, sizeof(minDt_check),
                    "{\"onsetDt\":%.6f,\"minDt\":%.6f,\"rejected\":%d,\"t_samples\":%llu,\"hypothesisId\":\"B\"}",
                    onsetDt, minDt, (onsetDt < minDt) ? 1 : 0, (unsigned long long)t_samples);
                debug_log(3, "TempoTracker.cpp:updateBeat", "minDt_gating", minDt_check, t_samples);
            }
            // #endregion
            
            // Beat-candidate gating: minimum interval check
            if (onsetDt < minDt) {
                // Out-of-range fast onset - do NOT reset beat IOI clock
                // This prevents hats from stealing the beat clock
                return;  // Reject this onset as beat-candidate
            }

            // Clamp plausible period
            float minP = 60.0f / tuning_.maxBpm;
            float maxP = 60.0f / tuning_.minBpm;

            if (onsetDt >= minP && onsetDt <= maxP) {
                // All valid intervals contribute to density buffer (no consistency gating)
                diagnostics_.intervalsValid++;
                diagnostics_.lastValidInterval = onsetDt;
                
                // Log valid interval at verbosity 4 (for debugging interval acceptance)
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 4) {
                    float candidateBpm = 60.0f / onsetDt;
                    uint64_t onsetDtUs = static_cast<uint64_t>(onsetDt * 1e6f);
                    char valid_data[512];
                    snprintf(valid_data, sizeof(valid_data),
                        "{\"interval\":%.3f,\"intervalUs\":%llu,\"bpm\":%.1f,"
                        "\"minP\":%.3f,\"maxP\":%.3f,\"minBpm\":%.1f,\"maxBpm\":%.1f,"
                        "\"density_bins_updated\":3,\"t_samples\":%llu,\"lastOnsetSamples\":%llu}",
                        onsetDt, (unsigned long long)onsetDtUs, candidateBpm,
                        minP, maxP, tuning_.minBpm, tuning_.maxBpm,
                        (unsigned long long)t_samples, (unsigned long long)beat_state_.lastOnsetUs);
                    debug_log(4, "TempoTracker.cpp:updateBeat", "interval_valid", valid_data, t_samples);
                }
                
                // Compute candidate BPM
                float candidateBpm = 60.0f / onsetDt;
                
                // Add octave variants: 0.5×, 1×, 2× (when in range)
                float variants[] = {candidateBpm * 0.5f, candidateBpm, candidateBpm * 2.0f};
                
                for (int oct = 0; oct < 3; oct++) {
                    float bpm = variants[oct];
                    if (bpm >= BeatState::DENSITY_MIN_BPM && bpm <= BeatState::DENSITY_MAX_BPM) {
                        // Find bin index
                        int bin = static_cast<int>(bpm - BeatState::DENSITY_MIN_BPM + 0.5f);
                        if (bin >= 0 && bin < BeatState::DENSITY_BINS) {
                            // Add triangular kernel (2 BPM width)
                            float kernelWidth = 2.0f;
                            for (int offset = -2; offset <= 2; offset++) {
                                int targetBin = bin + offset;
                                if (targetBin >= 0 && targetBin < BeatState::DENSITY_BINS) {
                                    float dist = std::abs(static_cast<float>(offset));
                                    float weight = std::max(0.0f, 1.0f - dist / kernelWidth);
                                    beat_state_.tempoDensity[targetBin] += weight;
                                }
                            }
                        }
                    }
                }
                
                // 2nd-order PLL correction on onset
                // Compute phase error (target is 0.0)
                float phaseError = beat_state_.phase01;  // Current phase (should be 0 at beat)
                
                // Clamp phase error to [-0.5, 0.5] range
                if (phaseError > 0.5f) phaseError -= 1.0f;
                if (phaseError < -0.5f) phaseError += 1.0f;
                
                // Update integral (with windup protection)
                beat_state_.phaseErrorIntegral += phaseError;
                const float maxIntegral = 2.0f;  // Prevent windup
                beat_state_.phaseErrorIntegral = std::max(-maxIntegral, 
                                                         std::min(maxIntegral, beat_state_.phaseErrorIntegral));
                
                // Proportional correction (phase)
                float phaseCorrection = beat_state_.pllKp * phaseError;
                phaseCorrection = std::max(-0.1f, std::min(0.1f, phaseCorrection));  // Clamp
                beat_state_.phase01 -= phaseCorrection;
                
                // Integral correction (tempo) - slow tempo correction
                // Note: Fast tempo updates come from density buffer winner in updateTempo()
                // PLL provides slow, continuous correction for phase alignment
                float tempoCorrection = beat_state_.pllKi * beat_state_.phaseErrorIntegral;
                tempoCorrection = std::max(-5.0f, std::min(5.0f, tempoCorrection));  // Clamp to ±5 BPM
                beat_state_.bpm += tempoCorrection;
                
                // Normalize phase
                if (beat_state_.phase01 < 0.0f) beat_state_.phase01 += 1.0f;
                if (beat_state_.phase01 >= 1.0f) beat_state_.phase01 -= 1.0f;
            } else {
                // Interval out of range - track it
                diagnostics_.intervalsRejected++;
                diagnostics_.lastRejectedInterval = onsetDt;
                
                // Log rejection reason at verbosity 4
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 4) {
                    float candidateBpm = 60.0f / onsetDt;
                    uint64_t onsetDtUs = static_cast<uint64_t>(onsetDt * 1e6f);
                    char rej_data[512];
                    snprintf(rej_data, sizeof(rej_data),
                        "{\"interval\":%.3f,\"intervalUs\":%llu,\"bpm\":%.1f,"
                        "\"min_bpm\":%.1f,\"max_bpm\":%.1f,\"minP\":%.3f,\"maxP\":%.3f,"
                        "\"reason\":\"%s\",\"t_samples\":%llu,\"lastOnsetSamples\":%llu}",
                        onsetDt, (unsigned long long)onsetDtUs, candidateBpm,
                        tuning_.minBpm, tuning_.maxBpm, minP, maxP,
                        (onsetDt < minP) ? "too_fast" : "too_slow",
                        (unsigned long long)t_samples, (unsigned long long)beat_state_.lastOnsetUs);
                    debug_log(4, "TempoTracker.cpp:updateBeat", "interval_rejected", rej_data, t_samples);
                }
            }
        }
        beat_state_.lastOnsetUs = t_samples;  // Store in samples
    }
    
    // Update phase based on current BPM estimate (from density buffer)
    float currentPeriod = 60.0f / (beat_state_.bpm + 1e-6f);
    beat_state_.phase01 += dt / currentPeriod;
    
    // Normalize phase
    if (beat_state_.phase01 >= 1.0f) {
        beat_state_.phase01 -= 1.0f;
    }
}

// ============================================================================
// Phase Advancement
// ============================================================================

void TempoTracker::advancePhase(float delta_sec, uint64_t t_samples) {
    // Fix beat_tick generation: use stored last_phase_ from previous call (persist across calls)
    float old_phase = last_phase_;  // Previous phase from last call
    float new_phase = beat_state_.phase01;  // Current phase
    
    // Update stored phase for next call
    last_phase_ = new_phase;

    // Phase already advanced in updateBeat()
    // This call is kept for AudioNode compatibility, but phase integration
    // happens in updateTempo() -> updateBeat()

    // Beat tick detection: zero crossing from high to low (phase wraps 1->0)
    // Use old_phase vs new_phase (not same value)
    beat_tick_ = (old_phase > 0.9f && new_phase < 0.1f);

    // Debounce: prevent multiple ticks within 60% of beat period
    if (beat_tick_) {
        float beat_period_samples = (60.0f / beat_state_.bpm) * 16000.0f;  // samples
        if (last_tick_samples_ > 0 && (t_samples - last_tick_samples_) < static_cast<uint64_t>(beat_period_samples * 0.6f)) {
            beat_tick_ = false;  // Too soon, suppress
        } else {
            last_tick_samples_ = t_samples;
            
            // Track phase jitter
            if (diagnostics_.isLocked) {
                beat_state_.beatTickHistory[beat_state_.beatTickHistoryIdx] = t_samples;
                beat_state_.beatTickHistoryIdx = (beat_state_.beatTickHistoryIdx + 1) % 10;
                
                // Compute phase jitter (deviation from expected period)
                if (beat_state_.beatTickHistoryIdx == 0) {  // Full cycle
                    float expectedPeriod = 60.0f / beat_state_.bpm * 16000.0f;  // samples
                    float jitterSum = 0.0f;
                    for (int i = 1; i < 10; i++) {
                        float actualPeriod = static_cast<float>(beat_state_.beatTickHistory[i] - beat_state_.beatTickHistory[i-1]);
                        float error = actualPeriod - expectedPeriod;
                        jitterSum += error * error;
                    }
                    diagnostics_.phaseJitter = std::sqrt(jitterSum / 9.0f) / 16.0f;  // Convert to ms (samples/16 = ms at 16kHz)
                }
            }
        }
    }
}

// ============================================================================
// Layer 3: Output Formatting
// ============================================================================

TempoOutput TempoTracker::getOutput() const {
    return TempoOutput{
        .bpm = beat_state_.bpm,
        .phase01 = beat_state_.phase01,
        .confidence = beat_state_.conf,
        .beat_tick = beat_tick_,
        .locked = beat_state_.conf > 0.2f,  // Locked if confidence > 20%
        .beat_strength = onset_strength_    // Use last onset strength as beat strength
    };
}

} // namespace audio
} // namespace lightwaveos
