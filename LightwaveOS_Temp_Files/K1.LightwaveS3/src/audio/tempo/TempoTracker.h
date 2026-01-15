/**
 * @file TempoTracker.h
 * @brief Onset-timing tempo tracker for LightwaveOS v2
 *
 * Features:
 * - Onset detection from spectral flux + VU derivative
 * - Beat tracking via inter-onset interval timing
 * - Phase-locked loop for beat alignment
 * - Confidence based on onset consistency
 *
 * Architecture (3 layers):
 * - Layer 1: Onset detection (8-band spectral flux + RMS derivative)
 * - Layer 2: Beat tracking (onset timing → BPM estimation + phase lock)
 * - Layer 3: Output formatting (BeatState → TempoOutput compatibility)
 *
 * Design goals:
 * - <1 KB memory footprint (down from 22 KB)
 * - No harmonic aliasing (155→77→81 BPM jumps eliminated)
 * - Musical saliency-based onset detection
 * - Stable tempo lock with PLL-based phase correction
 *
 * @author LightwaveOS Team
 * @version 2.0.0 - Onset-timing rewrite
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include "../contracts/TempoOutput.h"
#include "../k1/K1Types.h"

namespace lightwaveos {
namespace audio {

// Forward declarations
struct AudioFeatureFrame;

// ============================================================================
// State Machine
// ============================================================================

/**
 * @brief Tempo tracker state machine states
 *
 * Represents the tracker's confidence and behavior mode:
 * - INITIALIZING: Gathering initial data (first 50 hops)
 * - SEARCHING: Low confidence, sensitive onset detection
 * - LOCKING: Building confidence, moderate thresholds
 * - LOCKED: High confidence, selective onset detection
 * - UNLOCKING: Losing confidence, preparing to search again
 */
enum class TempoTrackerState {
    INITIALIZING,   ///< Just started, gathering initial data
    SEARCHING,      ///< Looking for tempo, low confidence
    LOCKING,        ///< Building confidence, tempo hypothesis forming
    LOCKED,         ///< High confidence, tempo stable
    UNLOCKING       ///< Confidence dropping, losing lock
};

// ============================================================================
// Configuration
// ============================================================================

struct TempoTrackerTuning {
    // ========================================
    // BPM RANGE
    // ========================================
    float minBpm = 60.0f;                ///< Minimum detectable BPM
    float maxBpm = 300.0f;               ///< Maximum detectable BPM (matches refractory period: 60/0.2s = 300 BPM)

    // ========================================
    // ONSET DETECTION
    // ========================================
    float onsetThreshK = 1.8f;           ///< Multiplier over baseline for onset (lowered for better sensitivity)
    uint32_t refractoryMs = 200;         ///< P0-F FIX: Minimum time between onsets (ms) - increased to 200ms (300 BPM max) to prevent subdivisions
    float baselineAlpha = 0.22f;         ///< Baseline smoothing (EMA alpha) (increased for faster adaptation)
    float minBaselineInit = 0.001f;      ///< Minimum baseline floor to prevent decay to zero
    float minBaselineVu = 0.001f;        ///< Minimum VU baseline floor
    float minBaselineSpec = 0.001f;      ///< Minimum spectral baseline floor

    // ========================================
    // PHASE 4: ADAPTIVE THRESHOLD (SYNESTHESIA)
    // ========================================
    float adaptiveThresholdSensitivity = 1.5f;  ///< Phase 4: Sensitivity multiplier for std dev (Synesthesia: 1.5)

    // ========================================
    // FLUX COMBINATION
    // ========================================
    float fluxWeightVu = 0.5f;           ///< Weight for VU delta in combined flux (50/50 with spectral)
    float fluxWeightSpec = 0.5f;         ///< Weight for spectral flux in combined flux
    float fluxNormalizedMax = 10.0f;     ///< Maximum normalized flux value (clamp outliers)
    float fluxBaselineEps = 1e-6f;       ///< Epsilon for baseline division (prevent divide-by-zero)

    // ========================================
    // BEAT TRACKING
    // ========================================
    float lockStrength = 0.35f;          ///< Phase correction gain [0.0-1.0] (DEPRECATED - not used)
    float confRise = 0.1f;               ///< Confidence rise per good onset (slower rise)
    float confFall = 0.2f;               ///< Confidence fall per second without support
    float lockThreshold = 0.5f;          ///< Confidence threshold for "locked" state

    // ========================================
    // BPM SMOOTHING (PHASE 5: SYNESTHESIA EXPONENTIAL SMOOTHING)
    // ========================================
    float bpmAlphaAttack = 0.15f;        ///< Phase 5: Attack coefficient (BPM increasing)
    float bpmAlphaRelease = 0.05f;       ///< Phase 5: Release coefficient (BPM decreasing)
    float bpmAlpha = 0.1f;               ///< Legacy EMA smoothing factor (deprecated in Phase 5)

    // ========================================
    // CONFIDENCE CALCULATION (PHASE 6: MULTI-FACTOR)
    // ========================================
    float confAlpha = 0.2f;              ///< Confidence EMA smoothing factor

    // Phase 6: Multi-factor confidence weights (Synesthesia formula)
    float confWeightOnsetStrength = 0.4f;   ///< Weight for onset strength factor
    float confWeightTempoConsistency = 0.3f; ///< Weight for tempo consistency (low CoV)
    float confWeightStability = 0.2f;      ///< Weight for sustained density (votes)
    float confWeightPhaseCoherence = 0.1f; ///< Weight for phase alignment

    // ========================================
    // DENSITY BUFFER
    // ========================================
    float densityDecay = 0.995f;         ///< Density buffer decay per hop (0.995 = 0.5% decay/sec at 125 Hz, ~200s time constant)

    // ========================================
    // INTERVAL VOTING
    // ========================================
    float kernelWidth = 2.0f;            ///< Triangular kernel width for density voting (BPM bins)
    float octaveVariantWeight = 0.5f;    ///< Weight for octave variants (0.5×, 2×) during search mode

    // ========================================
    // PLL (PHASE-LOCKED LOOP)
    // ========================================
    float pllKp = 0.1f;                  ///< PLL proportional gain (phase correction)
    float pllKi = 0.01f;                 ///< PLL integral gain (tempo correction)
    float pllMaxIntegral = 2.0f;         ///< PLL integral windup limit
    float pllMaxPhaseCorrection = 0.1f;  ///< Maximum phase correction per onset (clamp)
    float pllMaxTempoCorrection = 5.0f;  ///< Maximum tempo correction per onset (BPM, clamp)

    // ========================================
    // PHASE ADVANCEMENT
    // ========================================
    float phaseWrapHighThreshold = 0.9f; ///< High threshold for beat tick detection (wrap from high to low)
    float phaseWrapLowThreshold = 0.1f;  ///< Low threshold for beat tick detection
    float beatTickDebounce = 0.6f;       ///< Debounce factor (60% of beat period minimum)

    // ========================================
    // LOW-CONFIDENCE RESET
    // ========================================
    float lowConfThreshold = 0.15f;      ///< Confidence below which we consider "lost"
    float lowConfResetTimeSec = 8.0f;    ///< Seconds of low confidence before soft-reset
    float densitySoftResetFactor = 0.3f; ///< Multiply density buffer by this on soft-reset (not full clear)

    // ========================================
    // INTERVAL MISMATCH RESET
    // ========================================
    float intervalMismatchThreshold = 10.0f; ///< BPM difference to trigger mismatch check
    uint8_t intervalMismatchCount = 5;       ///< Number of consecutive mismatched intervals before reset

    // ========================================
    // INTERVAL WEIGHTING (CONSISTENCY BOOST)
    // ========================================
    float consistencyBoostThreshold = 15.0f; ///< BPM difference for consistency boost (within N BPM = boost)
    float consistencyBoostMultiplier = 3.0f; ///< Multiply weight by this if interval matches recent ones
    uint8_t recentIntervalWindow = 5;        ///< Number of recent intervals to check for consistency

    // ========================================
    // OCTAVE FLIP DETECTION
    // ========================================
    float octaveFlipRatioHigh = 1.8f;    ///< Ratio threshold for octave flip detection (near 2×)
    float octaveFlipRatioLow = 0.55f;    ///< Ratio threshold for octave flip detection (near 0.5×)

    // ========================================
    // OUTLIER REJECTION (P1-D)
    // ========================================
    float outlierStdDevThreshold = 2.0f; ///< Standard deviation threshold for outlier rejection
    float outlierMinConfidence = 0.3f;   ///< Minimum confidence to enable outlier rejection

    // ========================================
    // ONSET STRENGTH WEIGHTING (P1-A)
    // ========================================
    float onsetStrengthWeightBase = 1.0f;    ///< Base weight for onset strength
    float onsetStrengthWeightScale = 0.5f;   ///< Scale factor for onset strength (1.0-3.5× range)

    // ========================================
    // CONDITIONAL OCTAVE VOTING (P1-B)
    // ========================================
    float octaveVotingConfThreshold = 0.3f;  ///< Confidence threshold - vote octaves only below this

    // ========================================
    // INTERVAL VALIDATION
    // ========================================
    float periodAlpha = 0.15f;           ///< EMA alpha for period estimation
    float periodInitSec = 0.5f;          ///< Initial period estimate (120 BPM = 0.5 sec)

    // ========================================
    // K1 FRONT-END INITIALIZATION
    // ========================================
    float k1BaselineInit = 1.0f;         ///< K1 normalized baseline initialization (novelty ≈ 1.0)
    float k1BaselineAlpha = 0.05f;       ///< K1 baseline adaptation alpha (5% new, 95% history)
    float k1BaselineCheckThreshold = 0.1f; ///< Threshold to detect legacy baselines (< 0.1 = reinit)

    // ========================================
    // PEAK GATING
    // ========================================
    float peakGatingCapMultiplier = 1.5f; ///< Cap peak contributions to prevent baseline contamination

    // ========================================
    // ONSET STRENGTH LIMITS
    // ========================================
    float onsetStrengthMin = 0.0f;       ///< Minimum onset strength (clamped)
    float onsetStrengthMax = 5.0f;       ///< Maximum onset strength (clamped)

    // ========================================
    // TRIANGULAR KERNEL WEIGHTS (P1-A, updateTempo overload)
    // ========================================
    float kernelWeightCenter = 1.0f;     ///< Weight for center bin in triangular kernel
    float kernelWeightPlus1 = 0.5f;      ///< Weight for ±1 bin
    float kernelWeightPlus2 = 0.25f;     ///< Weight for ±2 bin

    // ========================================
    // SPECTRAL WEIGHTS
    // ========================================
    // Spectral flux weights (8 bands, reduced disparity to detect weak beats)
    // Reduced from 4.67x to 2.4x disparity per research document recommendation
    float spectralWeights[8] = {1.2f, 1.1f, 1.0f, 0.8f, 0.7f, 0.5f, 0.5f, 0.5f};
};

// ============================================================================
// State Structures
// ============================================================================

/**
 * @brief Onset detector state
 *
 * Combines spectral flux and VU derivative to produce a single onset signal.
 * Implements Synesthesia-style adaptive threshold (Phase 3-4).
 */
struct OnsetState {
    float baseline_vu;                  ///< EMA baseline for VU derivative
    float baseline_spec;                 ///< EMA baseline for spectral flux
    float flux_prev;                     ///< Previous combined flux (for peak detection)
    float flux_prevprev;                 ///< Previous-previous combined flux (for peak detection)
    uint64_t lastOnsetUs;                ///< Time of last onset (samples, sample counter)
    float bands_last[8];                 ///< Last 8-band values for spectral flux
    float rms_last;                      ///< Last RMS for VU derivative

    // Phase 3-4: Adaptive threshold with flux history
    static constexpr uint8_t FLUX_HISTORY_SIZE = 40;  ///< 40 frames ≈ 230ms @ 173 Hz hop rate
    float flux_history[FLUX_HISTORY_SIZE];            ///< Circular buffer of recent flux values
    uint8_t flux_history_idx;                         ///< Write index for circular buffer
    uint8_t flux_history_count;                       ///< Number of valid entries (0-40)
};

/**
 * @brief Beat tracker state
 *
 * Tracks BPM, phase, and confidence based on inter-onset intervals.
 * Implements Synesthesia-style exponential smoothing (Phase 5).
 */
struct BeatState {
    float bpm;                           ///< Current estimated BPM (smoothed)
    float bpm_raw;                       ///< Raw BPM estimate (before smoothing, Phase 5)
    float bpm_prev;                      ///< Previous smoothed BPM (for attack/release, Phase 5)
    float phase01;                       ///< Phase [0, 1) - 0 = beat instant
    float conf;                          ///< Confidence [0, 1]

    uint64_t lastUs;                     ///< Last update time (microseconds)
    uint64_t lastOnsetUs;                ///< Last onset time (microseconds)

    float periodSecEma;                  ///< EMA of inter-onset period (seconds)
    float periodAlpha;                   ///< EMA alpha for period estimation
    
    // Tempo correction state
    uint32_t correctionCheckCounter;     ///< Counter for periodic tempo correction checks
    float lastCorrectionBpm;            ///< BPM before last correction (to detect oscillation)
    
    // Interval history for pattern detection
    float recentIntervals[5];            ///< Rolling window of last 5 valid intervals (ORIGINAL, not mutated)
    uint8_t intervalCount;               ///< Number of intervals in window (0-5)
    
    // Tempo density buffer (60-180 BPM, 1 BPM bins = 121 bins)
    static constexpr int DENSITY_BINS = 121;
    static constexpr float DENSITY_MIN_BPM = 60.0f;
    static constexpr float DENSITY_MAX_BPM = 180.0f;
    float tempoDensity[DENSITY_BINS];  ///< Tempo density histogram
    float densityDecay = 0.995f;        ///< Decay factor per hop (Phase C: 0.995 = 0.5% decay/sec at 125 Hz, ~200s time constant)
    
    // 2nd-order PLL state
    float phaseErrorIntegral;     ///< Integral of phase error (for tempo correction)
    float pllKp = 0.1f;           ///< Proportional gain (phase correction)
    float pllKi = 0.01f;          ///< Integral gain (tempo correction)
    
    // BPM jitter tracking
    float bpmHistory[10];  ///< Last 10 BPM values for jitter calculation
    uint8_t bpmHistoryIdx;  ///< Current index in history
    
    // Phase jitter tracking
    uint64_t beatTickHistory[10];  ///< Last 10 beat tick times (us)
    uint8_t beatTickHistoryIdx;    ///< Current index
    
    // Octave flip detection
    float lastBpmFromDensity;  ///< Previous BPM from density buffer
    
    // Low-confidence reset tracking
    uint64_t lowConfStartSamples;  ///< Sample counter when confidence dropped below threshold (0 = not tracking)
    
    // Interval mismatch tracking
    uint8_t intervalMismatchCounter;  ///< Count of consecutive intervals that disagree with density peak
};

/**
 * @brief TempoTracker diagnostic state
 *
 * Tracks detailed metrics for debugging beat tracking issues.
 */
struct TempoTrackerDiagnostics {
    // Onset detection stats
    uint32_t onsetCount;              ///< Total onsets detected
    uint32_t onsetRejectedRefractory; ///< Rejected due to refractory period
    uint32_t onsetRejectedThreshold;  ///< Rejected due to threshold
    float lastOnsetInterval;          ///< Last inter-onset interval (seconds)
    uint64_t lastOnsetTime;           ///< Timestamp of last onset
    
    // Flux/threshold tracking
    float currentFlux;                ///< Current combined flux value
    float baseline;                   ///< Current EMA baseline
    float threshold;                  ///< Current threshold (baseline * threshK)
    
    // Interval validation
    uint32_t intervalsValid;          ///< Count of valid intervals (in range and consistent)
    uint32_t intervalsRejected;       ///< Count of rejected intervals (out of range or inconsistent)
    uint32_t intervalsRejectedInconsistent; ///< Count rejected due to inconsistency (in range but not matching current BPM)
    uint32_t intervals_rej_too_fast;  ///< P0.5: Count rejected due to too fast (< minBeatInterval)
    uint32_t intervals_rej_too_slow;  ///< P0.5: Count rejected due to too slow (> maxBeatInterval)
    float lastValidInterval;          ///< Last valid inter-onset interval
    float lastRejectedInterval;       ///< Last rejected interval (for debugging)
    
    // Confidence tracking
    uint32_t confidenceRises;         ///< Count of confidence increases
    uint32_t confidenceFalls;          ///< Count of confidence decays
    float lastConfidenceDelta;        ///< Last confidence change
    
    // Acceptance criteria metrics
    uint64_t lockTimeMs;           ///< Time to first confidence > 0.5 (ms)
    float bpmJitter;              ///< RMS BPM variation (after lock)
    float phaseJitter;             ///< RMS phase timing error (ms, after lock)
    uint32_t octaveFlips;         ///< Count of half/double corrections
    bool isLocked;                ///< Currently locked (conf > 0.5)
    uint64_t lockStartTime;       ///< Time when lock achieved

    // Phase 4 additions
    float intervalStdDev;        ///< Standard deviation of recent intervals
    float intervalCoV;           ///< Coefficient of variation
    int mismatchStreak;          ///< Current consecutive mismatch count
    int votesInWinnerBin;        ///< Votes in density buffer winner bin

    // Phase 5 additions
    TempoTrackerState currentState;   ///< Current state machine state
    uint32_t hopCount;                ///< Total hops since init
    uint8_t activeIntervalCount;      ///< Non-expired intervals
    bool enabled = false;             ///< Enable diagnostic logging
};

// ============================================================================
// Output Structure (Moved to contracts/TempoOutput.h)
// ============================================================================


// ============================================================================
// TempoTracker Class
// ============================================================================

/**
 * @brief Onset-timing tempo tracker
 *
 * Architecture:
 *   Layer 1: Onset Detection
 *     8-band pre-AGC + RMS -> spectral flux + VU derivative -> combined onset signal
 *
 *   Layer 2: Beat Tracking
 *     onset timing -> inter-onset interval -> BPM estimation + PLL phase lock
 *
 *   Layer 3: Output Formatting
 *     BeatState -> TempoOutput (6 fields for effect compatibility)
 *
 * Call sequence per audio frame:
 *   1. updateNovelty() - onset detection (spectral flux + VU derivative)
 *   2. updateTempo()   - beat tracking (inter-onset timing + phase lock)
 *   3. advancePhase()  - phase integration
 *   4. getOutput()     - read current state
 *
 * Memory: <1 KB total (down from 22 KB)
 *   - OnsetState: ~48 bytes
 *   - BeatState: ~48 bytes
 *   - No history buffers
 */
class TempoTracker {
public:
    /**
     * @brief Initialize all state
     *
     * Must be called before any other method.
     * Computes Goertzel coefficients and clears buffers.
     */
    void init();

    /**
     * @brief Update tuning parameters
     * @param tuning New tuning configuration
     */
    void setTuning(const TempoTrackerTuning& tuning) { tuning_ = tuning; }

    /**
     * @brief Update onset detection from K1 features
     *
     * Uses rhythm_novelty as primary onset evidence (already scale-invariant from K1).
     *
     * @param frame K1 AudioFeatureFrame with rhythm_novelty and rhythm_energy
     */
    void updateFromFeatures(const k1::AudioFeatureFrame& frame);

    /**
     * @brief Update onset detection from 8-band Goertzel and RMS (legacy, for compatibility)
     *
     * @param bands 8-band Goertzel magnitudes (can be nullptr if not ready)
     * @param num_bands Number of bands (8)
     * @param rms Current RMS value [0,1]
     * @param bands_ready True when fresh 8-band data available
     * @param tMicros Current time in microseconds (for determinism)
     */
    void updateNovelty(const float* bands, uint8_t num_bands, float rms, bool bands_ready, uint64_t tMicros);

    /**
     * @brief Update novelty from unified onset strength (Phase 2 Integration)
     *
     * Accepts pre-computed onset strength from dual-bank analysis.
     *
     * @param onsetStrength Unified onset strength [0.0, ∞) from AudioFeatureFrame
     * @param t_samples Current time in samples (sample counter, deterministic)
     */
    void updateNovelty(float onsetStrength, uint64_t t_samples);

    /**
     * @brief Update beat tracking from onset signal (legacy)
     *
     * If onset detected, updates BPM estimate from inter-onset interval
     * and applies phase-locked loop correction. Updates confidence.
     *
     * @param delta_sec Time since last call (for phase advancement)
     * @param t_samples Current time in samples (sample counter, deterministic)
     */
    void updateTempo(float delta_sec, uint64_t t_samples);

    /**
     * @brief Update beat tracking from AudioFeatureFrame (Phase 2 Integration)
     *
     * Implements 4 critical onset fixes:
     * - P1-A: Onset strength weighting
     * - P1-B: Conditional octave voting
     * - P1-C: Harmonic filtering
     * - P1-D: Outlier rejection
     *
     * @param frame Unified audio feature frame with rhythm/harmony flux and chroma
     * @param t_samples Current time in samples (sample counter, deterministic)
     */
    void updateTempo(const AudioFeatureFrame& frame, uint64_t t_samples);

    /**
     * @brief Advance beat phase
     *
     * Integrates phase at current BPM rate and detects beat ticks.
     * This is called separately from updateTempo() to match existing
     * AudioNode call sequence.
     *
     * @param delta_sec Time since last call
     * @param t_samples Current time in samples (sample counter, deterministic)
     */
    void advancePhase(float delta_sec, uint64_t t_samples);

    /**
     * @brief Get current output state
     *
     * @return TempoOutput with BPM, phase, confidence, beat_tick
     */
    TempoOutput getOutput() const;

    float getNovelty() const { return 0.0f; } // Placeholder for compatibility with debug logging

    // ========================================================================
    // Debug Accessors (compatibility shims for legacy code)
    // ========================================================================

    /**
     * @brief Get tempo bins (compatibility shim - returns nullptr)
     * Legacy accessor for Goertzel bins - no longer applicable.
     */
    const void* getBins() const { return nullptr; }

    /**
     * @brief Get smoothed magnitudes (compatibility shim - returns nullptr)
     * Legacy accessor for Goertzel spectrum - no longer applicable.
     */
    const float* getSmoothed() const { return nullptr; }

    /**
     * @brief Get winner bin (compatibility shim - returns 0)
     * Legacy accessor for Goertzel winner - no longer applicable.
     */
    uint16_t getWinnerBin() const { return 0; }

    /**
     * @brief Get onset strength (new diagnostic accessor)
     */
    float getOnsetStrength() const { return onset_strength_; }

    /**
     * @brief Get last onset flag (new diagnostic accessor)
     */
    bool getLastOnset() const { return last_onset_; }

    /**
     * @brief Get diagnostic state
     * @return Reference to diagnostic metrics
     */
    const TempoTrackerDiagnostics& getDiagnostics() const { return diagnostics_; }

private:
    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * @brief Detect onset from spectral flux and VU derivative
     *
     * Updates onset_state_ and returns onset flag and strength.
     *
     * @param flux Combined spectral flux + VU derivative
     * @param tMicros Current time in microseconds
     * @param outStrength Output onset strength (0-5+)
     * @return True if onset detected
     */
    bool detectOnset(float flux, uint64_t t_samples, float& outStrength);

    /**
     * @brief Calculate adaptive threshold (Phase 4: Synesthesia formula)
     *
     * Implements: threshold = median(flux_history) + 1.5 * σ(flux_history)
     *
     * @return Adaptive threshold value
     */
    float calculateAdaptiveThreshold() const;

    /**
     * @brief Calculate median of flux history buffer
     *
     * @return Median flux value from last 40 frames
     */
    float calculateFluxMedian() const;

    /**
     * @brief Calculate standard deviation of flux history buffer
     *
     * @param median Pre-calculated median value (for efficiency)
     * @return Standard deviation of flux history
     */
    float calculateFluxStdDev(float median) const;

    /**
     * @brief Apply exponential smoothing to BPM (Phase 5: Synesthesia formula)
     *
     * Uses attack/release coefficients:
     * - α_attack = 0.15 (when BPM increasing)
     * - α_release = 0.05 (when BPM decreasing)
     *
     * @param raw_bpm Raw BPM estimate from IOI analysis
     * @return Smoothed BPM value
     */
    float applyBpmSmoothing(float raw_bpm);

    /**
     * @brief Update beat tracking from onset
     *
     * Updates beat_state_ with BPM estimate and phase correction.
     *
     * @param onset True if onset detected this frame
     * @param onsetStrength Onset strength (0-5+)
     * @param tMicros Current time in microseconds
     * @param delta_sec Time since last call
     */
    void updateBeat(bool onset, float onsetStrength, uint64_t t_samples, float delta_sec);

    /**
     * @brief Calculate standard deviation of recent intervals
     * @return Standard deviation in seconds
     */
    float calculateRecentIntervalsStdDev() const;

    /**
     * @brief Calculate coefficient of variation (CoV) of recent intervals
     * @return CoV = stdDev / mean
     */
    float calculateRecentIntervalsCoV() const;

    /**
     * @brief Count votes in a density buffer bin
     * @param binIndex Bin index to count
     * @return Vote count
     */
    int countVotesInBin(int binIndex) const;

    /**
     * @brief Find true second peak in density buffer (excluding winner's kernel)
     * @param excludePeakIdx Winner peak index to exclude
     * @return Second peak density value
     */
    float findTrueSecondPeak(int excludePeakIdx) const;

    /**
     * @brief Update state machine based on current confidence
     *
     * Implements 5-state machine transitions based on confidence thresholds
     * and timeout conditions.
     */
    void updateState();

    /**
     * @brief Get state-dependent onset threshold multiplier
     * @param base_threshold Base threshold to modify
     * @return Adjusted threshold based on current state
     */
    float getStateDependentOnsetThreshold(float base_threshold) const;

    /**
     * @brief Get state-dependent BPM smoothing alpha
     * @return Alpha value adapted to current state
     */
    float getStateDependentBpmAlpha() const;

    /**
     * @brief Get recency weight for interval voting
     * @param interval_index Index of interval (0 = oldest, N-1 = newest)
     * @param total_intervals Total number of valid intervals
     * @return Weight factor [0.5, 1.0]
     */
    float getRecencyWeight(uint8_t interval_index, uint8_t total_intervals) const;

    /**
     * @brief Add interval to history with timestamp
     * @param interval Interval duration in seconds
     * @param timestamp Sample timestamp
     */
    void addInterval(float interval, uint64_t timestamp);

    /**
     * @brief Calculate phase coherence factor (Phase 6)
     *
     * Measures alignment between predicted beat phase and current phase.
     * Ranges from 0 (antiphase) to 1.0 (perfect alignment).
     *
     * @return Phase coherence factor [0.0-1.0]
     */
    float calculatePhaseCoherence() const;

    /**
     * @brief Calculate onset strength factor (Phase 6)
     *
     * Normalizes recent onset magnitudes relative to baseline.
     *
     * @param onsetFlux Current onset flux value
     * @return Normalized strength [0.0-1.0+]
     */
    float calculateOnsetStrengthFactor(float onsetFlux) const;

    /**
     * @brief Expire old intervals (> 10 seconds old)
     * @param current_time Current sample timestamp
     */
    void expireOldIntervals(uint64_t current_time);

    /**
     * @brief Count active (non-expired) intervals
     * @return Number of valid intervals in history
     */
    uint8_t countActiveIntervals() const;

private:
    // ========================================================================
    // State Variables
    // ========================================================================

    TempoTrackerTuning tuning_;

    // Layer 1: Onset detection state (~48 bytes)
    OnsetState onset_state_;
    bool last_onset_;                    ///< Last onset flag (for diagnostics)
    float onset_strength_;               ///< Last onset strength (for diagnostics)
    float combined_flux_;                ///< Combined spectral + VU flux

    // Layer 2: Beat tracking state (~48 bytes)
    BeatState beat_state_;

    // Output state
    bool beat_tick_;                     ///< Beat tick flag (zero-crossing detection)
    float last_phase_;                    ///< Previous phase value (persisted across calls for wrap detection)
    uint64_t last_tick_samples_;          ///< Time of last beat tick in samples (for debouncing)

    // Diagnostic state
    TempoTrackerDiagnostics diagnostics_; ///< Diagnostic metrics for debugging
    uint64_t m_initTime;                  ///< Initialization time (for lock time tracking)

    // Periodic summary logging
    uint32_t summaryLogCounter_;  ///< Counter for periodic summary logs
    static constexpr uint32_t SUMMARY_LOG_INTERVAL = 62;  ///< Log every 62 hops (~1 second at 62.5 Hz)

    // Phase 4: Mismatch streak tracking
    int mismatch_streak_;  ///< Consecutive mismatches between hypothesis and density winner

    // Phase 5: State machine tracking
    TempoTrackerState state_ = TempoTrackerState::INITIALIZING;  ///< Current state machine state
    uint32_t hop_count_ = 0;  ///< Total hops since init (for timeout detection)

    // Phase 5: Interval timestamp tracking (16-element circular buffer)
    float recentIntervalsExtended_[16];      ///< Extended interval history for expiration
    uint64_t recentIntervalTimestamps_[16];  ///< Timestamps for each recent interval
    uint8_t recentIntervalIndex_ = 0;        ///< Write index for recent intervals
};

} // namespace audio
} // namespace lightwaveos
