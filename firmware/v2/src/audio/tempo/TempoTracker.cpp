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
#include "../AudioNode.h"  // For AudioFeatureFrame
#include "../../config/audio_config.h"  // For HOP_SIZE and SAMPLE_RATE
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include "../AudioDebugConfig.h"

// #region agent log
// ANSI colour codes for terminal readability
#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_WHITE   "\033[37m"
#define ANSI_BOLD    "\033[1m"

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

// Coloured log for human-readable tempo events (separate from JSON)
// Always print (verbosity >= 1) because tempo intervals are critical for debugging
static void tempo_event_log(const char* colour, const char* tag, const char* format, ...) {
    auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
    if (dbgCfg.verbosity < 1) {
        return;
    }
    printf("%s[%s]%s ", colour, tag, ANSI_RESET);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}
// #endregion

namespace lightwaveos {
namespace audio {

// ============================================================================
// Initialization
// ============================================================================

void TempoTracker::init() {
    // Initialize onset detection state
    // P0-E FIX: Baseline initialization depends on K1 vs legacy mode
#ifdef FEATURE_K1_FRONT_END
    // K1 normalized range (novelty ≈ 0.5-6.0)
    onset_state_.baseline_vu = tuning_.k1BaselineInit;
    onset_state_.baseline_spec = tuning_.k1BaselineInit;
#else
    // Legacy range (flux ≈ 0.01-0.1)
    onset_state_.baseline_vu = 0.01f;
    onset_state_.baseline_spec = 0.01f;
#endif
    // Safety check: ensure they're at least at minimum floor
    if (onset_state_.baseline_vu < tuning_.minBaselineInit) onset_state_.baseline_vu = tuning_.minBaselineInit;
    if (onset_state_.baseline_spec < tuning_.minBaselineInit) onset_state_.baseline_spec = tuning_.minBaselineInit;
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
    beat_state_.periodSecEma = tuning_.periodInitSec;    // 120 BPM = 0.5 sec period
    beat_state_.periodAlpha = tuning_.periodAlpha;
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
    
    // Initialize low-confidence reset tracking
    beat_state_.lowConfStartSamples = 0;
    
    // Initialize interval mismatch tracking
    beat_state_.intervalMismatchCounter = 0;

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

    // Initialize Phase 4 state
    mismatch_streak_ = 0;

    // Initialize Phase 5 state
    state_ = TempoTrackerState::INITIALIZING;
    hop_count_ = 0;
    memset(recentIntervalsExtended_, 0, sizeof(recentIntervalsExtended_));
    memset(recentIntervalTimestamps_, 0, sizeof(recentIntervalTimestamps_));
    recentIntervalIndex_ = 0;
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

    float vu_thresh = onset_state_.baseline_vu * tuning_.onsetThreshK;
    float baseline_vu_before = onset_state_.baseline_vu;
    if (vu_delta <= vu_thresh) {
        // Normal update when below threshold
        onset_state_.baseline_vu = (1.0f - tuning_.baselineAlpha) * onset_state_.baseline_vu +
                                    tuning_.baselineAlpha * vu_delta;
        // Enforce minimum floor
        if (onset_state_.baseline_vu < tuning_.minBaselineVu) {
            onset_state_.baseline_vu = tuning_.minBaselineVu;
        }
    } else {
        // Peak gating: cap contribution to prevent contamination
        // Use max of current baseline or minimum to ensure recovery from near-zero
        float effective_baseline = std::max(onset_state_.baseline_vu, tuning_.minBaselineVu);
        float capped_vu = std::min(vu_delta, effective_baseline * tuning_.peakGatingCapMultiplier);
        onset_state_.baseline_vu = (1.0f - tuning_.baselineAlpha) * onset_state_.baseline_vu +
                                    tuning_.baselineAlpha * capped_vu;
        // Enforce minimum floor
        if (onset_state_.baseline_vu < tuning_.minBaselineVu) {
            onset_state_.baseline_vu = tuning_.minBaselineVu;
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
            if (onset_state_.baseline_spec < tuning_.minBaselineSpec) {
                onset_state_.baseline_spec = tuning_.minBaselineSpec;
            }
        } else {
            // Peak gating: cap contribution to prevent contamination
            // Use max of current baseline or minimum to ensure recovery from near-zero
            float effective_baseline = std::max(onset_state_.baseline_spec, tuning_.minBaselineSpec);
            float capped_spec = std::min(spectral_flux, effective_baseline * tuning_.peakGatingCapMultiplier);
            onset_state_.baseline_spec = (1.0f - tuning_.baselineAlpha) * onset_state_.baseline_spec +
                                          tuning_.baselineAlpha * capped_spec;
            // Enforce minimum floor
            if (onset_state_.baseline_spec < tuning_.minBaselineSpec) {
                onset_state_.baseline_spec = tuning_.minBaselineSpec;
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
    float vu_n = vu_delta / (onset_state_.baseline_vu + tuning_.fluxBaselineEps);
    float spec_n = (bands_ready && spectral_flux > 0.0f) ?
                   spectral_flux / (onset_state_.baseline_spec + tuning_.fluxBaselineEps) : 0.0f;

    // Clamp normalized values to prevent extreme outliers
    vu_n = std::max(0.0f, std::min(tuning_.fluxNormalizedMax, vu_n));
    spec_n = std::max(0.0f, std::min(tuning_.fluxNormalizedMax, spec_n));

    // Combine with configurable weights (default 50/50)
    if (bands_ready) {
        combined_flux_ = tuning_.fluxWeightSpec * spec_n + tuning_.fluxWeightVu * vu_n;
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
    // #region Phase A: Build verification marker
    static bool k1_path_verified = false;
    if (!k1_path_verified) {
        debug_log(1, "TempoTracker.cpp:updateFromFeatures", "K1_TEMPO_TRACKER_V2", "K1 path active", frame.t_samples);
        k1_path_verified = true;
    }
    // #endregion
    
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
    
    // Phase B: Initialize K1-mode baselines to 1.0 (K1 novelty is normalized, baseline ≈ 1.0)
    // Legacy init sets baselines to 0.01, which is wrong for normalized K1 novelty
    static bool k1_baselines_initialized = false;
    if (!k1_baselines_initialized) {
        // Check if baselines are still at legacy init values (0.01)
        if (onset_state_.baseline_vu < tuning_.k1BaselineCheckThreshold && onset_state_.baseline_spec < tuning_.k1BaselineCheckThreshold) {
            onset_state_.baseline_vu = tuning_.k1BaselineInit;
            onset_state_.baseline_spec = tuning_.k1BaselineInit;
        }
        k1_baselines_initialized = true;
    }

    // Update combined flux from novelty (K1 already provides normalized novelty)
    combined_flux_ = novelty;

    // Baseline EMA: slow adaptation (alpha=0.05 means 5% new value, 95% history)
    onset_state_.baseline_spec = (1.0f - tuning_.k1BaselineAlpha) * onset_state_.baseline_spec + tuning_.k1BaselineAlpha * novelty;

    // Enforce minimum floor to prevent baseline from decaying to near-zero
    if (onset_state_.baseline_spec < tuning_.minBaselineSpec) {
        onset_state_.baseline_spec = tuning_.minBaselineSpec;
    }
    onset_state_.baseline_vu = onset_state_.baseline_spec;  // Keep them in sync for K1 mode
    
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
    float combined_baseline = (onset_state_.baseline_vu * tuning_.fluxWeightVu + onset_state_.baseline_spec * tuning_.fluxWeightSpec);
    // Phase B: For K1 mode, baseline ≈ 1.0, so threshold ≈ 1.8 (onsetThreshK = 1.8)
    // This works well for normalized novelty values of 1-6 (peaks should be >1.8)

    // Phase 4: Component 2 - Adaptive onset threshold based on confidence
    float base_threshold = combined_baseline * tuning_.onsetThreshK;
    // Phase 5: State-dependent threshold adjustment
    float state_adaptive_threshold = getStateDependentOnsetThreshold(base_threshold);
    float confidence_adaptive_threshold = state_adaptive_threshold * (0.5f + 0.5f * beat_state_.conf);
    // Low confidence (0.0) → 0.5× threshold (more sensitive)
    // High confidence (1.0) → 1.0× threshold (more selective)
    float thresh = confidence_adaptive_threshold;
    
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
        
        outStrength = (flux - thresh) / (thresh + tuning_.fluxBaselineEps);
        // Clamp strength to [0, 5]
        outStrength = std::max(tuning_.onsetStrengthMin, std::min(tuning_.onsetStrengthMax, outStrength));
        
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
// Phase 4: Interval Consistency Helpers
// ============================================================================

float TempoTracker::calculateRecentIntervalsStdDev() const {
    if (diagnostics_.intervalsValid < 2) return 0.0f;

    // Use recent intervals from beat_state_ (P1-D interval array)
    // Compute mean
    float mean = 0.0f;
    int count = std::min((int)diagnostics_.intervalsValid, (int)beat_state_.intervalCount);
    if (count == 0) return 0.0f;

    for (int i = 0; i < count; i++) {
        mean += beat_state_.recentIntervals[i];
    }
    mean /= count;

    // Compute variance
    float variance = 0.0f;
    for (int i = 0; i < count; i++) {
        float diff = beat_state_.recentIntervals[i] - mean;
        variance += diff * diff;
    }
    variance /= count;

    return sqrtf(variance);
}

float TempoTracker::calculateRecentIntervalsCoV() const {
    if (diagnostics_.intervalsValid < 2) return 1.0f;  // High variance when insufficient data

    float mean = 0.0f;
    int count = std::min((int)diagnostics_.intervalsValid, (int)beat_state_.intervalCount);
    if (count == 0) return 1.0f;

    for (int i = 0; i < count; i++) {
        mean += beat_state_.recentIntervals[i];
    }
    mean /= count;

    if (mean < 0.001f) return 1.0f;  // Avoid divide-by-zero

    float stdDev = calculateRecentIntervalsStdDev();
    return stdDev / mean;
}

int TempoTracker::countVotesInBin(int binIndex) const {
    if (binIndex < 0 || binIndex >= BeatState::DENSITY_BINS) return 0;

    // Sum votes in kernel around bin (triangular kernel width from tuning)
    int halfWidth = (int)(tuning_.kernelWidth);  // kernelWidth = 2.0f
    float totalVotes = 0.0f;

    for (int offset = -halfWidth; offset <= halfWidth; offset++) {
        int idx = binIndex + offset;
        if (idx >= 0 && idx < BeatState::DENSITY_BINS) {
            totalVotes += beat_state_.tempoDensity[idx];
        }
    }

    return (int)totalVotes;
}

float TempoTracker::findTrueSecondPeak(int excludePeakIdx) const {
    float secondMax = 0.0f;
    int halfWidth = (int)(tuning_.kernelWidth);  // kernelWidth = 2.0f

    for (int i = 0; i < BeatState::DENSITY_BINS; i++) {
        if (abs(i - excludePeakIdx) > halfWidth) {  // Outside winner's kernel
            secondMax = std::max(secondMax, beat_state_.tempoDensity[i]);
        }
    }
    return secondMax;
}

// ============================================================================
// Layer 2: Beat Tracking
// ============================================================================

// ============================================================================
// Phase 2 Integration: Simplified Novelty Update
// ============================================================================

void TempoTracker::updateNovelty(float onsetStrength, uint64_t t_samples) {
    // Phase 2: Accept pre-computed onset strength from AudioFeatureFrame
    // (70% rhythm + 30% harmony via getOnsetStrength())

    // Update baseline with same logic as existing updateNovelty
    float baseline_before = onset_state_.baseline_vu;  // Use VU baseline for unified signal

    float thresh = baseline_before * tuning_.onsetThreshK;
    if (onsetStrength <= thresh) {
        // Normal baseline update
        onset_state_.baseline_vu = (1.0f - tuning_.baselineAlpha) * baseline_before +
                                    tuning_.baselineAlpha * onsetStrength;
        if (onset_state_.baseline_vu < tuning_.minBaselineVu) {
            onset_state_.baseline_vu = tuning_.minBaselineVu;
        }
    } else {
        // Peak gating: cap contribution
        float effective_baseline = std::max(baseline_before, tuning_.minBaselineVu);
        float capped = std::min(onsetStrength, effective_baseline * tuning_.peakGatingCapMultiplier);
        onset_state_.baseline_vu = (1.0f - tuning_.baselineAlpha) * baseline_before +
                                    tuning_.baselineAlpha * capped;
        if (onset_state_.baseline_vu < tuning_.minBaselineVu) {
            onset_state_.baseline_vu = tuning_.minBaselineVu;
        }
    }

    // Store current novelty for onset detection
    combined_flux_ = onsetStrength;
    onset_strength_ = onsetStrength / (onset_state_.baseline_vu + tuning_.fluxBaselineEps);

    // Detect onset
    bool onset_now = (onset_strength_ > tuning_.onsetThreshK);

    // Refractory period check
    uint64_t refract_samples = (tuning_.refractoryMs * 16000ULL) / 1000;
    if (onset_now && (t_samples - onset_state_.lastOnsetUs) < refract_samples) {
        onset_now = false;  // Suppress onset in refractory period
    }

    // Store onset state
    last_onset_ = onset_now;
    if (onset_now) {
        onset_state_.lastOnsetUs = t_samples;
    }
}

// ============================================================================
// Legacy Tempo Update (delta_sec based)
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
    
    // Phase D: Removed redundant updateBeat() call - updateFromFeatures() already called updateBeat()
    // Density buffer voting happens in the updateBeat() call from updateFromFeatures(), not here
    
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
    // Phase 4: Use findTrueSecondPeak helper to find true second peak (not kernel shoulder)
    float secondPeak = findTrueSecondPeak(peakBin);

    // Estimate BPM from peak
    float bpm_hat = BeatState::DENSITY_MIN_BPM + static_cast<float>(peakBin);

    // Compute confidence from peak sharpness
    float peak_sharpness = (maxDensity - secondPeak) / (maxDensity + tuning_.fluxBaselineEps);
    peak_sharpness = std::max(0.0f, std::min(1.0f, peak_sharpness));

    // Phase 4: Component 1 - Apply interval consistency factor
    float consistency_factor = 1.0f - std::min(calculateRecentIntervalsCoV(), 1.0f);
    // High variance (CoV=1.0) → consistency_factor=0.0
    // Low variance (CoV=0.0) → consistency_factor=1.0

    float raw_confidence = peak_sharpness * consistency_factor;

    // Smooth BPM estimate (EMA)
    // Phase 4: Component 2 - Adaptive BPM smoothing
    // Phase 5: Use state-dependent BPM alpha
    float state_bpm_alpha = getStateDependentBpmAlpha();
    float adaptive_bpm_alpha = (beat_state_.conf < 0.3f) ? 0.2f : state_bpm_alpha;
    beat_state_.bpm = (1.0f - adaptive_bpm_alpha) * beat_state_.bpm + adaptive_bpm_alpha * bpm_hat;

    // Update confidence from density (with temporal smoothing)
    beat_state_.conf = (1.0f - tuning_.confAlpha) * beat_state_.conf + tuning_.confAlpha * raw_confidence;

    // Phase 4: Component 4 - Gradual confidence build-up (cap until sustained evidence)
    int votes_in_winner_bin = countVotesInBin(peakBin);

    if (votes_in_winner_bin < 10) {
        // Not enough sustained evidence, cap confidence
        beat_state_.conf = std::min(beat_state_.conf, 0.3f);
    }

    if (votes_in_winner_bin < 5) {
        // Minimum vote threshold - no confidence
        beat_state_.conf = 0.0f;
    }

    // Track lock time
    if (beat_state_.conf > tuning_.lockThreshold && !diagnostics_.isLocked) {
        diagnostics_.isLocked = true;
        diagnostics_.lockStartTime = t_samples;
        if (diagnostics_.lockTimeMs == 0) {
            // First lock - record time from init (convert samples to ms)
            diagnostics_.lockTimeMs = ((t_samples - m_initTime) * 1000ULL) / 16000;
        }
    } else if (beat_state_.conf <= tuning_.lockThreshold && diagnostics_.isLocked) {
        diagnostics_.isLocked = false;
    }
    
    // =========================================================================
    // LOW-CONFIDENCE RESET MECHANISM
    // If confidence stays below threshold for N seconds, soft-reset density buffer
    // This allows re-acquisition when tempo changes significantly (e.g., new song)
    // =========================================================================
    if (beat_state_.conf < tuning_.lowConfThreshold) {
        // Start or continue tracking low-confidence duration
        if (beat_state_.lowConfStartSamples == 0) {
            beat_state_.lowConfStartSamples = t_samples;
        } else {
            // Check if we've been low-confidence long enough to trigger reset
            float lowConfDurationSec = static_cast<float>(t_samples - beat_state_.lowConfStartSamples) / 16000.0f;
            if (lowConfDurationSec >= tuning_.lowConfResetTimeSec) {
                // SOFT RESET: Reduce density buffer to allow new tempo to emerge
                // Don't fully clear - keep some history to avoid cold-start issues
                for (int i = 0; i < BeatState::DENSITY_BINS; i++) {
                    beat_state_.tempoDensity[i] *= tuning_.densitySoftResetFactor;
                }
                
                // Reset tracking state
                beat_state_.lowConfStartSamples = 0;
                
                // Log the reset event (ANSI cyan for visibility)
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 2) {
                    printf("\033[36m[TEMPO RESET]\033[0m Soft-reset density buffer after %.1fs low confidence (conf=%.2f < %.2f)\n",
                           lowConfDurationSec, beat_state_.conf, tuning_.lowConfThreshold);
                }
                
                // Also log as DEBUG_JSON for parsing
                char reset_data[256];
                snprintf(reset_data, sizeof(reset_data),
                    "{\"reason\":\"low_confidence_timeout\",\"duration_sec\":%.1f,\"conf\":%.2f,\"threshold\":%.2f,\"reset_factor\":%.2f}",
                    lowConfDurationSec, beat_state_.conf, tuning_.lowConfThreshold, tuning_.densitySoftResetFactor);
                debug_log(2, "TempoTracker.cpp:updateTempo", "density_soft_reset", reset_data, t_samples);
            }
        }
    } else {
        // Confidence is above threshold - reset the low-confidence timer
        beat_state_.lowConfStartSamples = 0;
    }
    
    // Detect octave flips (large BPM jumps)
    if (beat_state_.lastBpmFromDensity > 0.0f) {
        float ratio = bpm_hat / beat_state_.lastBpmFromDensity;
        if (ratio > tuning_.octaveFlipRatioHigh || ratio < tuning_.octaveFlipRatioLow) {  // Near 2x or 0.5x
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
    
    // Update Phase 4 diagnostics
    diagnostics_.intervalStdDev = calculateRecentIntervalsStdDev();
    diagnostics_.intervalCoV = calculateRecentIntervalsCoV();
    diagnostics_.mismatchStreak = mismatch_streak_;
    diagnostics_.votesInWinnerBin = votes_in_winner_bin;

    // Periodic summary log (verbosity >= 3, every ~1 second)
    summaryLogCounter_++;
    if (summaryLogCounter_ >= SUMMARY_LOG_INTERVAL) {
        summaryLogCounter_ = 0;
        
        auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
        if (dbgCfg.verbosity >= 3) {
            char summary_data[768];
            snprintf(summary_data, sizeof(summary_data),
                "{\"bpm\":%.1f,\"bpm_hat\":%.1f,\"conf\":%.2f,\"locked\":%d,"
                "\"density_peak_bin\":%d,\"density_peak_val\":%.5f,\"density_second_peak\":%.5f,"
                "\"onsets_total\":%u,\"onsets_rej_refr\":%u,\"onsets_rej_thr\":%u,"
                "\"intervals_valid\":%u,\"intervals_rej\":%u,\"intervals_rej_too_fast\":%u,\"intervals_rej_too_slow\":%u,\"rejection_rate_pct\":%.1f,"
                "\"last_valid_interval\":%.3f,\"last_valid_bpm\":%.1f,"
                "\"bpm_jitter\":%.2f,\"phase_jitter_ms\":%.1f,\"octave_flips\":%u,"
                "\"lock_time_ms\":%llu,"
                "\"interval_stddev\":%.4f,\"interval_cov\":%.4f,\"mismatch_streak\":%d,\"votes_in_winner\":%d}",
                beat_state_.bpm, bpm_hat, beat_state_.conf, diagnostics_.isLocked ? 1 : 0,
                peakBin, maxDensity, secondPeak,
                diagnostics_.onsetCount, diagnostics_.onsetRejectedRefractory, diagnostics_.onsetRejectedThreshold,
                diagnostics_.intervalsValid, diagnostics_.intervalsRejected,
                diagnostics_.intervals_rej_too_fast, diagnostics_.intervals_rej_too_slow,
                (diagnostics_.intervalsValid + diagnostics_.intervalsRejected > 0) ?
                    (100.0f * diagnostics_.intervalsRejected / (diagnostics_.intervalsValid + diagnostics_.intervalsRejected)) : 0.0f,
                diagnostics_.lastValidInterval,
                (diagnostics_.lastValidInterval > 0.0f) ? (60.0f / diagnostics_.lastValidInterval) : 0.0f,
                diagnostics_.bpmJitter, diagnostics_.phaseJitter, diagnostics_.octaveFlips,
                diagnostics_.lockTimeMs,
                diagnostics_.intervalStdDev, diagnostics_.intervalCoV, diagnostics_.mismatchStreak, diagnostics_.votesInWinnerBin);
            debug_log(3, "TempoTracker.cpp:updateTempo", "tempo_summary", summary_data, t_samples);
            
            // Human-readable coloured summary (CYAN = summary, YELLOW if unlocked, GREEN if locked)
            float rejRate = (diagnostics_.intervalsValid + diagnostics_.intervalsRejected > 0) ?
                (100.0f * diagnostics_.intervalsRejected / (diagnostics_.intervalsValid + diagnostics_.intervalsRejected)) : 0.0f;
            const char* lockColour = diagnostics_.isLocked ? ANSI_GREEN : ANSI_YELLOW;
            const char* lockStatus = diagnostics_.isLocked ? "LOCKED" : "UNLOCKED";
            printf("%s[TEMPO]%s BPM=%.1f conf=%.2f %s%s%s valid=%u rej=%u (%.0f%%) peak_bin=%d\n",
                   ANSI_CYAN, ANSI_RESET,
                   beat_state_.bpm, beat_state_.conf,
                   lockColour, lockStatus, ANSI_RESET,
                   diagnostics_.intervalsValid, diagnostics_.intervalsRejected, rejRate, peakBin);
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

    // Phase 5: Periodic interval expiration (every 1 second @ 8ms/hop)
    if (hop_count_ % 125 == 0) {
        expireOldIntervals(t_samples);
    }

    // Phase 5: Update state machine based on current confidence
    updateState();

    // Phase 5: Update diagnostics with state information
    diagnostics_.currentState = state_;
    diagnostics_.hopCount = hop_count_;
    diagnostics_.activeIntervalCount = countActiveIntervals();
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

    // Phase D: Removed duplicate phase advancement (was here, now only at end of function)
    // Phase advancement happens once at end of updateBeat() to avoid 4× per hop corruption

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
        // P0-C FIX: Prevent Onset Poisoning (TIGHTENED)
        // Only process onsets in beat-range intervals (0.333-1.0s = 180-60 BPM)
        // This is the 99.7% rejection rate fix

        // Estimate beat period from time since last onset
        if (beat_state_.lastOnsetUs != 0) {
            float onsetDt = static_cast<float>(t_samples - beat_state_.lastOnsetUs) / 16000.0f;

            // TIGHTENED: 180 BPM max (not 333 BPM max)
            const float minBeatInterval = 60.0f / tuning_.maxBpm;  // ~0.333s (180 BPM)
            const float maxBeatInterval = 60.0f / tuning_.minBpm;  // ~1.0s (60 BPM)

            // Only process onsets in plausible beat range
            if (onsetDt >= minBeatInterval && onsetDt <= maxBeatInterval) {
                // ... existing interval validation logic continues below ...
            } else {
                // Log rejection but DO NOT update lastOnsetUs
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 3) {
                    char rej_data[256];
                    const char* reason = (onsetDt < minBeatInterval) ? "too_fast" : "too_slow";
                    snprintf(rej_data, sizeof(rej_data),
                        "{\"interval\":%.3f,\"minBeatInterval\":%.3f,\"maxBeatInterval\":%.3f,\"reason\":\"%s\",\"hypothesisId\":\"C\"}",
                        onsetDt, minBeatInterval, maxBeatInterval, reason);
                    debug_log(3, "TempoTracker.cpp:updateBeat", "onset_rejected_poisoning", rej_data, t_samples);
                }

                // P0.5: Track rejection counters for diagnostics
                if (onsetDt < minBeatInterval) {
                    diagnostics_.intervals_rej_too_fast++;
                    diagnostics_.intervalsRejected++;
                } else {
                    diagnostics_.intervals_rej_too_slow++;
                    diagnostics_.intervalsRejected++;
                }
                diagnostics_.lastRejectedInterval = onsetDt;

                // CRITICAL: DO NOT update lastOnsetUs - this prevents onset poisoning
                return;
            }

            // #region agent log
            static uint32_t minDt_log_counter = 0;
            if ((minDt_log_counter++ % 50) == 0) {  // Log every 50th check
                char minDt_check[256];
                snprintf(minDt_check, sizeof(minDt_check),
                    "{\"onsetDt\":%.6f,\"minBeatInterval\":%.6f,\"maxBeatInterval\":%.6f,\"accepted\":true,\"t_samples\":%llu,\"hypothesisId\":\"C\"}",
                    onsetDt, minBeatInterval, maxBeatInterval, (unsigned long long)t_samples);
                debug_log(3, "TempoTracker.cpp:updateBeat", "onset_accepted_beat_range", minDt_check, t_samples);
            }
            // #endregion

            // Clamp plausible period
            float minP = 60.0f / tuning_.maxBpm;
            float maxP = 60.0f / tuning_.minBpm;

            if (onsetDt >= minP && onsetDt <= maxP) {
                // All valid intervals contribute to density buffer (no consistency gating)
                diagnostics_.intervalsValid++;
                diagnostics_.lastValidInterval = onsetDt;
                
                // Compute candidate BPM
                float candidateBpm = 60.0f / onsetDt;
                
                // #region agent log
                uint64_t onsetDtUs = static_cast<uint64_t>(onsetDt * 1e6f);
                char valid_data[512];
                snprintf(valid_data, sizeof(valid_data),
                    "{\"interval\":%.3f,\"intervalUs\":%llu,\"bpm\":%.1f,"
                    "\"minP\":%.3f,\"maxP\":%.3f,\"minBpm\":%.1f,\"maxBpm\":%.1f,"
                    "\"t_samples\":%llu,\"lastOnsetSamples\":%llu,\"hypothesisId\":\"C,D\"}",
                    onsetDt, (unsigned long long)onsetDtUs, candidateBpm,
                    minP, maxP, tuning_.minBpm, tuning_.maxBpm,
                    (unsigned long long)t_samples, (unsigned long long)beat_state_.lastOnsetUs);
                debug_log(3, "TempoTracker.cpp:updateBeat", "interval_valid", valid_data, t_samples);
                
                // Human-readable coloured log (GREEN = valid interval)
                tempo_event_log(ANSI_GREEN, "VALID", "interval=%.3fs -> %.1f BPM (voting into density)", onsetDt, candidateBpm);
                
                // Phase 4: Component 3 - Smarter reset logic (sustained hypothesis shift)
                // Check for interval mismatch with current density peak
                // Requires 10 consecutive mismatches (not just 5)
                if (beat_state_.lastBpmFromDensity > 0.0f) {
                    float bpmDifference = std::abs(candidateBpm - beat_state_.lastBpmFromDensity);
                    if (bpmDifference > tuning_.intervalMismatchThreshold) {
                        // Hypothesis disagrees with density peak
                        mismatch_streak_++;
                        if (mismatch_streak_ >= 10) {
                            // SOFT RESET: Sustained hypothesis shift (10 consecutive mismatches)
                            // Reset confidence
                            beat_state_.conf *= tuning_.densitySoftResetFactor;

                            // Clear interval history
                            for (int i = 0; i < 5; i++) {
                                beat_state_.recentIntervals[i] = 0.0f;
                            }
                            diagnostics_.intervalsValid = 0;

                            mismatch_streak_ = 0;

                            // Log the reset event
                            auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                            if (dbgCfg.verbosity >= 2) {
                                printf("\033[36m[TEMPO RESET]\033[0m Sustained hypothesis shift: intervals (%.1f BPM) disagree with peak (%.1f BPM) by %.1f BPM for 10 consecutive onsets\n",
                                       candidateBpm, beat_state_.lastBpmFromDensity, bpmDifference);
                            }
                            char reset_data[256];
                            snprintf(reset_data, sizeof(reset_data),
                                "{\"reason\":\"sustained_mismatch\",\"candidate_bpm\":%.1f,\"peak_bpm\":%.1f,\"difference\":%.1f,\"consecutive_mismatches\":10}",
                                candidateBpm, beat_state_.lastBpmFromDensity, bpmDifference);
                            debug_log(2, "TempoTracker.cpp:updateBeat", "tempo_reset_sustained", reset_data, t_samples);
                        }
                    } else {
                        // Interval agrees with density peak - reset streak
                        mismatch_streak_ = 0;
                    }
                }
                // #endregion
                
                // CONSISTENCY BOOST: Weight intervals that match recent ones more heavily
                // This helps clusters of similar intervals (like 136-144 BPM) dominate over random noise
                float baseWeight = 1.0f;
                float consistencyBoost = 1.0f;
                if (beat_state_.intervalCount > 0) {
                    // Check if this interval is similar to recent valid intervals
                    int matchCount = 0;
                    for (uint8_t i = 0; i < beat_state_.intervalCount && i < tuning_.recentIntervalWindow; i++) {
                        float recentBpm = 60.0f / beat_state_.recentIntervals[i];
                        float bpmDiff = std::abs(candidateBpm - recentBpm);
                        if (bpmDiff <= tuning_.consistencyBoostThreshold) {
                            matchCount++;
                        }
                    }
                    // Boost weight if this interval matches recent ones
                    if (matchCount > 0) {
                        consistencyBoost = tuning_.consistencyBoostMultiplier;
                        // Human-readable log for boosted intervals
                        auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                        if (dbgCfg.verbosity >= 2) {
                            printf("\033[33m[BOOST]\033[0m interval=%.3fs -> %.1f BPM matches %d recent intervals (weight ×%.1f)\n",
                                   onsetDt, candidateBpm, matchCount, consistencyBoost);
                        }
                    }
                }

                // Phase 5: Apply recency weight (most recent intervals vote more)
                // This is the time-weighted voting component
                float recency_weight = 1.0f;  // Default for newest interval
                // Note: Recency weighting is currently applied to the newest interval (recency_weight = 1.0)
                // For historical intervals, we would calculate their age-based weight
                // The current interval is always the most recent, so it gets full weight

                float intervalWeight = baseWeight * consistencyBoost * recency_weight;
                
                // Add octave variants: 0.5×, 1×, 2× (when in range)
                float variants[] = {candidateBpm * tuning_.octaveVariantWeight, candidateBpm, candidateBpm * (1.0f / tuning_.octaveVariantWeight)};

                float total_weight_added = 0.0f;
                int bins_updated = 0;

                for (int oct = 0; oct < 3; oct++) {
                    float bpm = variants[oct];
                    if (bpm >= BeatState::DENSITY_MIN_BPM && bpm <= BeatState::DENSITY_MAX_BPM) {
                        // Find bin index
                        int bin = static_cast<int>(bpm - BeatState::DENSITY_MIN_BPM + 0.5f);
                        if (bin >= 0 && bin < BeatState::DENSITY_BINS) {
                            // Add triangular kernel (2 BPM width)
                            for (int offset = -2; offset <= 2; offset++) {
                                int targetBin = bin + offset;
                                if (targetBin >= 0 && targetBin < BeatState::DENSITY_BINS) {
                                    float dist = std::abs(static_cast<float>(offset));
                                    float weight = std::max(0.0f, 1.0f - dist / tuning_.kernelWidth) * intervalWeight;
                                    float density_before = beat_state_.tempoDensity[targetBin];
                                    beat_state_.tempoDensity[targetBin] += weight;
                                    total_weight_added += weight;
                                    bins_updated++;
                                    
                                    // #region agent log
                                    if (bins_updated <= 3) {  // Log first 3 bin updates
                                        char density_update[256];
                                        snprintf(density_update, sizeof(density_update),
                                            "{\"bin\":%d,\"bpm\":%.1f,\"weight\":%.3f,\"density_before\":%.6f,\"density_after\":%.6f,\"hypothesisId\":\"D\"}",
                                            targetBin, bpm, weight, density_before, beat_state_.tempoDensity[targetBin]);
                                        debug_log(3, "TempoTracker.cpp:updateBeat", "density_update", density_update, t_samples);
                                    }
                                    // #endregion
                                }
                            }
                        }
                    }
                }
                
                // #region agent log
                char density_summary[256];
                snprintf(density_summary, sizeof(density_summary),
                    "{\"total_weight_added\":%.3f,\"bins_updated\":%d,\"candidateBpm\":%.1f,\"hypothesisId\":\"D\"}",
                    total_weight_added, bins_updated, candidateBpm);
                debug_log(3, "TempoTracker.cpp:updateBeat", "density_add_summary", density_summary, t_samples);
                // #endregion
                
                // 2nd-order PLL correction on onset
                // Compute phase error (target is 0.0)
                float target_phase = 0.0f;  // Beat instant
                float current_phase = beat_state_.phase01;

                // Phase 4: Component 5 - Fix phase error wrap-around via atan2
                float phase_diff = target_phase - current_phase;
                float phaseError = atan2(sin(2.0f * M_PI * phase_diff), cos(2.0f * M_PI * phase_diff)) / (2.0f * M_PI);
                // This ensures phase_error is in [-0.5, 0.5] range

                // Update integral (with adaptive windup protection)
                beat_state_.phaseErrorIntegral += phaseError;

                // Phase 4: Component 5 - Adaptive windup limit
                float adaptive_windup_limit = tuning_.pllMaxIntegral + (1.0f - beat_state_.conf) * 3.0f;
                // Low confidence (0.0): limit = 2.0 + 3.0 = 5.0 (higher limit when uncertain)
                // High confidence (1.0): limit = 2.0 + 0.0 = 2.0 (tighter limit when locked)
                beat_state_.phaseErrorIntegral = std::max(-adaptive_windup_limit,
                                                         std::min(adaptive_windup_limit, beat_state_.phaseErrorIntegral));

                // Proportional correction (phase)
                // Phase 4: Component 5 - Adaptive phase correction limit
                float adaptive_phase_correction_max = (beat_state_.conf < 0.5f) ? 0.2f : tuning_.pllMaxPhaseCorrection;
                float phaseCorrection = beat_state_.pllKp * phaseError;
                phaseCorrection = std::max(-adaptive_phase_correction_max, std::min(adaptive_phase_correction_max, phaseCorrection));
                beat_state_.phase01 -= phaseCorrection;

                // Integral correction (tempo) - slow tempo correction
                // Note: Fast tempo updates come from density buffer winner in updateTempo()
                // PLL provides slow, continuous correction for phase alignment
                // Phase 4: Component 5 - Adaptive tempo correction limit
                float adaptive_tempo_correction_max = (beat_state_.conf < 0.5f) ? 10.0f : tuning_.pllMaxTempoCorrection;
                float tempoCorrection = beat_state_.pllKi * beat_state_.phaseErrorIntegral;
                tempoCorrection = std::max(-adaptive_tempo_correction_max, std::min(adaptive_tempo_correction_max, tempoCorrection));
                beat_state_.bpm += tempoCorrection;
                
                // Normalize phase
                if (beat_state_.phase01 < 0.0f) beat_state_.phase01 += 1.0f;
                if (beat_state_.phase01 >= 1.0f) beat_state_.phase01 -= 1.0f;
                
                // Phase 5: Update recent intervals array with timestamp tracking
                // Use addInterval() helper to maintain both interval and timestamp
                addInterval(onsetDt, t_samples);

                // Also update the old 5-element array for compatibility
                // Shift existing intervals and add new one at the front
                for (int i = static_cast<int>(beat_state_.intervalCount) - 1; i >= 0 && i < 4; i--) {
                    beat_state_.recentIntervals[i + 1] = beat_state_.recentIntervals[i];
                }
                beat_state_.recentIntervals[0] = onsetDt;
                if (beat_state_.intervalCount < 5) {
                    beat_state_.intervalCount++;
                }
                
                // Commit accepted onset time (prevents hats/ghost onsets from poisoning intervals)
                beat_state_.lastOnsetUs = t_samples;  // ✅ Only update for accepted intervals
            } else {
                // Interval out of range - track it
                diagnostics_.intervalsRejected++;
                diagnostics_.lastRejectedInterval = onsetDt;
                
                float candidateBpm = 60.0f / onsetDt;
                const char* reason = (onsetDt < minP) ? "too_fast" : "too_slow";
                
                // Human-readable coloured log (RED = rejected interval)
                tempo_event_log(ANSI_RED, "REJECT", "interval=%.3fs -> %.1f BPM (%s, need %.0f-%.0f)", 
                               onsetDt, candidateBpm, reason, tuning_.minBpm, tuning_.maxBpm);
                
                // Log rejection reason at verbosity 4
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 4) {
                    uint64_t onsetDtUs = static_cast<uint64_t>(onsetDt * 1e6f);
                    char rej_data[512];
                    snprintf(rej_data, sizeof(rej_data),
                        "{\"interval\":%.3f,\"intervalUs\":%llu,\"bpm\":%.1f,"
                        "\"min_bpm\":%.1f,\"max_bpm\":%.1f,\"minP\":%.3f,\"maxP\":%.3f,"
                        "\"reason\":\"%s\",\"t_samples\":%llu,\"lastOnsetSamples\":%llu}",
                        onsetDt, (unsigned long long)onsetDtUs, candidateBpm,
                        tuning_.minBpm, tuning_.maxBpm, minP, maxP,
                        reason,
                        (unsigned long long)t_samples, (unsigned long long)beat_state_.lastOnsetUs);
                    debug_log(4, "TempoTracker.cpp:updateBeat", "interval_rejected", rej_data, t_samples);
                }
                
                // If interval is too slow, we likely missed beats; reset the onset timer
                // If it's too fast, DO NOT reset (avoids hats stealing the timer)
                if (onsetDt > maxP) {
                    beat_state_.lastOnsetUs = t_samples;  // ✅ Reset only for "too slow"
                }
                // ❌ Do NOT update lastOnsetUs for "too fast" - that's the bug
            }
        } else {
            // First onset - initialize timer
            beat_state_.lastOnsetUs = t_samples;  // Store in samples
        }
    }
    
    // Phase advancement is now handled in advancePhase() to avoid duplication
    // Removed phase advancement from here (was causing double advancement)
}

// ============================================================================
// Phase Advancement
// ============================================================================

void TempoTracker::advancePhase(float delta_sec, uint64_t t_samples) {
    // P0-A FIX: Store previous phase at START, then compare stored vs current
    float prev_phase = last_phase_;  // Use PREVIOUS stored value

    // Advance phase based on current BPM estimate (centralized phase advancement)
    float currentPeriod = 60.0f / (beat_state_.bpm + 1e-6f);
    beat_state_.phase01 += delta_sec / currentPeriod;

    // Normalize phase
    if (beat_state_.phase01 >= 1.0f) {
        beat_state_.phase01 -= 1.0f;
    }

    // Store current for NEXT call
    last_phase_ = beat_state_.phase01;

    // Beat tick detection: zero crossing from high to low (phase wraps 1->0)
    // Now compare previous vs current (from updateBeat)
    beat_tick_ = (prev_phase > tuning_.phaseWrapHighThreshold && beat_state_.phase01 < tuning_.phaseWrapLowThreshold);

    // Debounce: prevent multiple ticks within 60% of beat period
    if (beat_tick_) {
        float beat_period_samples = (60.0f / beat_state_.bpm) * 16000.0f;  // samples
        if (last_tick_samples_ > 0 && (t_samples - last_tick_samples_) < static_cast<uint64_t>(beat_period_samples * tuning_.beatTickDebounce)) {
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
    // P0-B FIX: Gate beat_tick by confidence threshold in getOutput()
    // This prevents the struct copy in AudioNode from overwriting the gating
    bool gated_beat_tick = beat_tick_ && (beat_state_.conf >= tuning_.lockThreshold);

    return TempoOutput{
        .bpm = beat_state_.bpm,
        .phase01 = beat_state_.phase01,
        .confidence = beat_state_.conf,
        .beat_tick = gated_beat_tick,  // Apply gating here
        .locked = beat_state_.conf >= tuning_.lockThreshold,
        .beat_strength = onset_strength_    // Use last onset strength as beat strength
    };
}

// ============================================================================
// Phase 2 Integration: AudioFeatureFrame-based Tempo Update
// Implements 4 Critical Onset Fixes
// ============================================================================

void TempoTracker::updateTempo(const AudioFeatureFrame& frame, uint64_t t_samples) {
    // This method replaces the old updateTempo(float delta_sec, uint64_t) call
    // It operates on the AudioFeatureFrame produced by dual-bank processing

    // Decay density buffer (same as legacy method)
    for (int i = 0; i < BeatState::DENSITY_BINS; i++) {
        beat_state_.tempoDensity[i] *= beat_state_.densityDecay;
    }

    // Only process onsets if we detected one
    if (!last_onset_) {
        // No onset - just maintain confidence decay and return
        float delta_sec = static_cast<float>(HOP_SIZE) / static_cast<float>(SAMPLE_RATE);
        float conf_decay_per_sec = tuning_.confFall;
        beat_state_.conf -= conf_decay_per_sec * delta_sec;
        if (beat_state_.conf < 0.0f) beat_state_.conf = 0.0f;
        return;
    }

    // ========================================================================
    // P1-D: OUTLIER REJECTION (Step 1: Compute statistics)
    // ========================================================================
    // Track recent interval statistics for outlier rejection
    static float recentIntervals[16] = {0};
    static uint8_t recentIndex = 0;

    // Compute interval from last onset
    float interval = static_cast<float>(t_samples - onset_state_.lastOnsetUs) / 16000.0f;

    // Compute mean and std dev from recent intervals
    float mean = 0.0f;
    for (uint8_t i = 0; i < 16; i++) {
        mean += recentIntervals[i];
    }
    mean /= 16.0f;

    float variance = 0.0f;
    for (uint8_t i = 0; i < 16; i++) {
        float diff = recentIntervals[i] - mean;
        variance += diff * diff;
    }
    float stdDev = sqrtf(variance / 16.0f);

    // Reject if > 2σ from mean (only when we have confidence)
    if (fabsf(interval - mean) > tuning_.outlierStdDevThreshold * stdDev && beat_state_.conf > tuning_.outlierMinConfidence) {
        diagnostics_.intervalsRejected++;
        // Don't vote - it's an outlier
        return;
    }

    // Store this interval in history
    recentIntervals[recentIndex] = interval;
    recentIndex = (recentIndex + 1) % 16;

    // ========================================================================
    // Convert interval to BPM
    // ========================================================================
    float bpm = 60.0f / interval;

    // Clamp to valid range
    if (bpm < tuning_.minBpm || bpm > tuning_.maxBpm) {
        diagnostics_.intervalsRejected++;
        return;
    }

    // ========================================================================
    // P1-A: ONSET STRENGTH WEIGHTING
    // ========================================================================
    // Weight votes by onset strength (1.0-3.5× range)
    float outStrength = frame.getOnsetStrength();  // Already weighted 70/30 rhythm/harmony
    float weight = tuning_.onsetStrengthWeightBase + (outStrength * tuning_.onsetStrengthWeightScale);  // 1.0-3.5× based on strength

    // ========================================================================
    // Vote into density buffer with triangular kernel (±2 bins)
    // ========================================================================
    int centerBin = static_cast<int>(bpm - BeatState::DENSITY_MIN_BPM);
    if (centerBin >= 0 && centerBin < BeatState::DENSITY_BINS) {
        // Triangular kernel: center gets full weight, ±1 gets 0.5×, ±2 gets 0.25×
        beat_state_.tempoDensity[centerBin] += weight * tuning_.kernelWeightCenter;

        if (centerBin > 0) {
            beat_state_.tempoDensity[centerBin - 1] += weight * tuning_.kernelWeightPlus1;
        }
        if (centerBin < BeatState::DENSITY_BINS - 1) {
            beat_state_.tempoDensity[centerBin + 1] += weight * tuning_.kernelWeightPlus1;
        }
        if (centerBin > 1) {
            beat_state_.tempoDensity[centerBin - 2] += weight * tuning_.kernelWeightPlus2;
        }
        if (centerBin < BeatState::DENSITY_BINS - 2) {
            beat_state_.tempoDensity[centerBin + 2] += weight * tuning_.kernelWeightPlus2;
        }
    }

    // ========================================================================
    // P1-B: CONDITIONAL OCTAVE VOTING
    // ========================================================================
    // ONLY vote octave variants when confidence < threshold (searching mode)
    if (beat_state_.conf < tuning_.octaveVotingConfThreshold) {
        // Vote 0.5× (half tempo - double interval)
        int idxHalf = static_cast<int>((bpm * tuning_.octaveVariantWeight) - BeatState::DENSITY_MIN_BPM);
        if (idxHalf >= 0 && idxHalf < BeatState::DENSITY_BINS) {
            beat_state_.tempoDensity[idxHalf] += weight * tuning_.octaveVariantWeight;
        }

        // Vote 2× (double tempo - half interval)
        int idxDouble = static_cast<int>((bpm * (1.0f / tuning_.octaveVariantWeight)) - BeatState::DENSITY_MIN_BPM);
        if (idxDouble >= 0 && idxDouble < BeatState::DENSITY_BINS) {
            beat_state_.tempoDensity[idxDouble] += weight * tuning_.octaveVariantWeight;
        }
    }
    // When confident (>= threshold), suppress octave variants entirely

    // ========================================================================
    // P1-C: HARMONIC FILTERING
    // ========================================================================
    // Use chroma stability for validation (future enhancement)
    // Currently: 70/30 rhythm/harmony weighting already applied in getOnsetStrength()
    // Future: When chromaStability > 0.8 and conf < 0.5, cross-check BPM against
    // chroma periodicity for additional validation
    (void)frame.chromaStability;  // Acknowledge for now

    // ========================================================================
    // Find peak bin and estimate BPM
    // ========================================================================
    float maxDensity = 0.0f;
    int peakBin = 0;
    for (int i = 0; i < BeatState::DENSITY_BINS; i++) {
        if (beat_state_.tempoDensity[i] > maxDensity) {
            maxDensity = beat_state_.tempoDensity[i];
            peakBin = i;
        }
    }

    // Find second peak (for confidence calculation)
    float secondPeak = 0.0f;
    for (int i = 0; i < BeatState::DENSITY_BINS; i++) {
        // Exclude peak neighborhood (±2 bins)
        if (i >= (peakBin - 2) && i <= (peakBin + 2)) {
            continue;
        }
        if (beat_state_.tempoDensity[i] > secondPeak) {
            secondPeak = beat_state_.tempoDensity[i];
        }
    }

    // Estimate BPM from peak
    float bpm_hat = BeatState::DENSITY_MIN_BPM + static_cast<float>(peakBin);

    // Compute confidence from peak sharpness
    float conf_from_density = (maxDensity - secondPeak) / (maxDensity + tuning_.fluxBaselineEps);
    conf_from_density = std::max(0.0f, std::min(1.0f, conf_from_density));

    // Smooth BPM estimate (EMA)
    beat_state_.bpm = (1.0f - tuning_.bpmAlpha) * beat_state_.bpm + tuning_.bpmAlpha * bpm_hat;

    // Update confidence from density (with temporal smoothing)
    beat_state_.conf = (1.0f - tuning_.confAlpha) * beat_state_.conf + tuning_.confAlpha * conf_from_density;

    // Track lock time
    if (beat_state_.conf > tuning_.lockThreshold && !diagnostics_.isLocked) {
        diagnostics_.isLocked = true;
        diagnostics_.lockStartTime = t_samples;
        if (diagnostics_.lockTimeMs == 0) {
            // First lock - record time from init
            diagnostics_.lockTimeMs = ((t_samples - m_initTime) * 1000ULL) / 16000;
        }
    } else if (beat_state_.conf <= tuning_.lockThreshold && diagnostics_.isLocked) {
        diagnostics_.isLocked = false;
    }

    // Low-confidence reset mechanism (same as legacy)
    if (beat_state_.conf < tuning_.lowConfThreshold) {
        if (beat_state_.lowConfStartSamples == 0) {
            beat_state_.lowConfStartSamples = t_samples;
        } else {
            float lowConfDurationSec = static_cast<float>(t_samples - beat_state_.lowConfStartSamples) / 16000.0f;
            if (lowConfDurationSec >= tuning_.lowConfResetTimeSec) {
                // Soft reset
                for (int i = 0; i < BeatState::DENSITY_BINS; i++) {
                    beat_state_.tempoDensity[i] *= tuning_.densitySoftResetFactor;
                }
                beat_state_.lowConfStartSamples = 0;

                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 2) {
                    printf("\033[36m[TEMPO RESET]\033[0m Soft-reset density buffer after %.1fs low confidence\n",
                           lowConfDurationSec);
                }
            }
        }
    } else {
        beat_state_.lowConfStartSamples = 0;
    }
}

// ============================================================================
// Phase 5: State Machine
// ============================================================================

void TempoTracker::updateState() {
    hop_count_++;  // Increment every hop

    switch (state_) {
        case TempoTrackerState::INITIALIZING:
            if (hop_count_ > 50) {  // 50 hops = 400ms @ 8ms/hop
                state_ = TempoTrackerState::SEARCHING;
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 2) {
                    printf("%s[STATE]%s INITIALIZING -> SEARCHING\n", ANSI_CYAN, ANSI_RESET);
                }
            }
            break;

        case TempoTrackerState::SEARCHING:
            if (beat_state_.conf > 0.3f) {
                state_ = TempoTrackerState::LOCKING;
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 2) {
                    printf("%s[STATE]%s SEARCHING -> LOCKING (conf=%.2f)\n", ANSI_CYAN, ANSI_RESET, beat_state_.conf);
                }
            }

            // 10-second timeout (Failure #75)
            if (hop_count_ > 1250) {  // 1250 hops = 10 seconds @ 8ms/hop
                // Reset and restart
                memset(beat_state_.tempoDensity, 0, sizeof(beat_state_.tempoDensity));
                beat_state_.conf = 0.0f;
                beat_state_.intervalCount = 0;
                hop_count_ = 0;
                state_ = TempoTrackerState::INITIALIZING;
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 2) {
                    printf("%s[STATE]%s SEARCHING -> INITIALIZING (timeout)\n", ANSI_YELLOW, ANSI_RESET);
                }
            }
            break;

        case TempoTrackerState::LOCKING:
            if (beat_state_.conf > tuning_.lockThreshold) {
                state_ = TempoTrackerState::LOCKED;
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 2) {
                    printf("%s[STATE]%s LOCKING -> LOCKED (conf=%.2f)\n", ANSI_GREEN, ANSI_RESET, beat_state_.conf);
                }
            }
            if (beat_state_.conf < 0.2f) {
                state_ = TempoTrackerState::SEARCHING;
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 2) {
                    printf("%s[STATE]%s LOCKING -> SEARCHING (conf=%.2f)\n", ANSI_YELLOW, ANSI_RESET, beat_state_.conf);
                }
            }
            break;

        case TempoTrackerState::LOCKED:
            if (beat_state_.conf < tuning_.lockThreshold * 0.8f) {
                state_ = TempoTrackerState::UNLOCKING;
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 2) {
                    printf("%s[STATE]%s LOCKED -> UNLOCKING (conf=%.2f)\n", ANSI_YELLOW, ANSI_RESET, beat_state_.conf);
                }
            }
            break;

        case TempoTrackerState::UNLOCKING:
            if (beat_state_.conf < 0.2f) {
                state_ = TempoTrackerState::SEARCHING;
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 2) {
                    printf("%s[STATE]%s UNLOCKING -> SEARCHING (conf=%.2f)\n", ANSI_YELLOW, ANSI_RESET, beat_state_.conf);
                }
            }
            if (beat_state_.conf > tuning_.lockThreshold) {  // Recovered
                state_ = TempoTrackerState::LOCKED;
                auto& dbgCfg = lightwaveos::audio::getAudioDebugConfig();
                if (dbgCfg.verbosity >= 2) {
                    printf("%s[STATE]%s UNLOCKING -> LOCKED (recovered, conf=%.2f)\n", ANSI_GREEN, ANSI_RESET, beat_state_.conf);
                }
            }
            break;
    }
}

// ============================================================================
// Phase 5: State-Dependent Behavior
// ============================================================================

float TempoTracker::getStateDependentOnsetThreshold(float base_threshold) const {
    switch (state_) {
        case TempoTrackerState::SEARCHING:
            return base_threshold * 0.8f;  // More sensitive
        case TempoTrackerState::LOCKING:
            return base_threshold;
        case TempoTrackerState::LOCKED:
            return base_threshold * 1.2f;  // More selective
        default:
            return base_threshold;
    }
}

float TempoTracker::getStateDependentBpmAlpha() const {
    switch (state_) {
        case TempoTrackerState::SEARCHING:
            return 0.2f;  // Faster smoothing when searching
        case TempoTrackerState::LOCKING:
            return 0.1f;  // Moderate smoothing when locking
        case TempoTrackerState::LOCKED:
            return 0.05f; // Slow smoothing when locked
        default:
            return tuning_.bpmAlpha;
    }
}

// ============================================================================
// Phase 5: Time-Weighted Voting
// ============================================================================

float TempoTracker::getRecencyWeight(uint8_t interval_index, uint8_t total_intervals) const {
    if (total_intervals == 0) return 1.0f;
    // Most recent = 1.0×, oldest = 0.5×
    return 0.5f + 0.5f * (float)interval_index / (float)total_intervals;
}

void TempoTracker::addInterval(float interval, uint64_t timestamp) {
    recentIntervalsExtended_[recentIntervalIndex_] = interval;
    recentIntervalTimestamps_[recentIntervalIndex_] = timestamp;
    recentIntervalIndex_ = (recentIntervalIndex_ + 1) % 16;
}

// ============================================================================
// Phase 5: Interval Expiration
// ============================================================================

void TempoTracker::expireOldIntervals(uint64_t current_time) {
    const uint64_t MAX_INTERVAL_AGE_SAMPLES = 160000ULL;  // 10 seconds in samples @ 16kHz (10 * 16000)

    for (uint8_t i = 0; i < 16; i++) {
        if (recentIntervalsExtended_[i] > 0.0f) {  // Non-zero = valid interval
            uint64_t age = current_time - recentIntervalTimestamps_[i];
            if (age > MAX_INTERVAL_AGE_SAMPLES) {
                // Expire this interval
                recentIntervalsExtended_[i] = 0.0f;
                recentIntervalTimestamps_[i] = 0;
                if (diagnostics_.intervalsValid > 0) {
                    diagnostics_.intervalsValid--;
                }
            }
        }
    }
}

uint8_t TempoTracker::countActiveIntervals() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < 16; i++) {
        if (recentIntervalsExtended_[i] > 0.0f) {
            count++;
        }
    }
    return count;
}

} // namespace audio
} // namespace lightwaveos
