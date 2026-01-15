/**
 * @file SignalQuality.h
 * @brief Audio signal quality metrics for pipeline observability
 * 
 * COMMERCIAL DEPLOYMENT REQUIREMENT
 * 
 * This module provides real-time signal quality metrics to enable visibility
 * into the audio pipeline health. Critical for debugging, tuning, and
 * commercial deployment validation.
 * 
 * FROM USER REQUIREMENT:
 * "We need GRANULAR oversight so we can work out whether the current DSP
 * design/architecture/algorithmic design is correct, whether it needs tuning,
 * or whether it needs to be completely binned and replaced."
 * 
 * SPECIFICATION DOCUMENTS:
 * - planning/audio-pipeline-redesign/prd.md (Problem Statement §2)
 * - planning/audio-pipeline-redesign/technical_blueprint_HARDENED.md §3.4
 * 
 * METRICS PROVIDED:
 * 1. DC Offset - SPH0645 microphone bias level
 * 2. Clipping Detection - Signal saturation events
 * 3. SNR Estimation - Signal-to-Noise Ratio approximation
 * 4. SPL Estimation - Sound Pressure Level approximation
 * 5. Signal Presence - Is there actual audio or just silence/noise?
 * 
 * WHY THIS MATTERS:
 * - DC offset: SPH0645 has fixed bias, need to verify it's correct
 * - Clipping: Indicates mic gain too high or ADC saturation
 * - SNR: Validates audio quality, detects noise floor issues
 * - SPL: Validates mic sensitivity and gain staging
 * - Presence: Detects "dead mic" or environmental silence
 * 
 * TIMING: Target <0.5ms per hop
 * MEMORY: ~4KB for history buffers
 * 
 * @version 1.0.0
 * @date 2026-01-13
 */

#pragma once

#include <cstdint>
#include <cmath>

namespace LightwaveOS {
namespace Audio {

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

/**
 * @brief History buffer length for quality metrics
 * WHY: 128 samples @ 8ms = ~1 second of history for averaging
 */
constexpr uint16_t QUALITY_HISTORY_LENGTH = 128;

/**
 * @brief Sample range for int16_t audio
 * INT16_MIN = -32768, INT16_MAX = 32767
 */
constexpr int16_t SAMPLE_MAX = 32767;
constexpr int16_t SAMPLE_MIN = -32768;

/**
 * @brief Clipping threshold (95% of max to account for near-clipping)
 * WHY: Samples consistently above 31000 indicate imminent clipping
 */
constexpr int16_t CLIPPING_THRESHOLD = 31000;

/**
 * @brief Silence threshold (absolute value)
 * WHY: Samples below this are likely just noise floor
 */
constexpr int16_t SILENCE_THRESHOLD = 100;

/**
 * @brief Noise floor estimate for SNR calculation (int16_t scale)
 * WHY: Typical SPH0645 + ESP32 I2S noise floor is ~50-100 LSB
 */
constexpr int16_t NOISE_FLOOR_ESTIMATE = 75;

// ============================================================================
// SIGNAL QUALITY METRICS STRUCTURE
// ============================================================================

/**
 * @brief Signal quality metrics snapshot
 * 
 * All metrics computed over most recent hop (128 samples @ 16kHz = 8ms)
 */
struct SignalQualityMetrics {
    // === DC Offset ===
    float dcOffset;                 ///< Average sample value (should be near 0 after correction)
    float dcOffsetRaw;              ///< Raw average before correction (SPH0645 bias level)
    
    // === Clipping ===
    uint16_t clippingCount;         ///< Number of samples near saturation
    float clippingPercent;          ///< Percentage of samples clipping [0, 100]
    bool isClipping;                ///< True if clipping detected (>5% of samples)
    
    // === Signal Level ===
    float rms;                      ///< RMS amplitude (root mean square)
    float peak;                     ///< Peak amplitude (max absolute value)
    float crestFactor;              ///< Peak / RMS ratio (dynamic range indicator)
    
    // === Noise & Quality ===
    float snrEstimate;              ///< Estimated SNR in dB (signal / noise floor)
    float splEstimate;              ///< Estimated SPL in dB (relative to full scale)
    bool signalPresent;             ///< True if signal above silence threshold
    
    // === Activity ===
    uint32_t hopCount;              ///< Number of hops processed (for averaging)
    uint32_t silentHopCount;        ///< Number of consecutive silent hops
};

// ============================================================================
// SIGNAL QUALITY ANALYZER CLASS
// ============================================================================

/**
 * @brief Real-time audio signal quality analyzer
 * 
 * Provides observability metrics for:
 * - Microphone health (DC offset, noise floor)
 * - Signal integrity (clipping, dynamic range)
 * - Audio presence (silence detection, SNR)
 * 
 * USAGE:
 * 1. Call init() once at startup
 * 2. Call update(samples, count) every audio hop
 * 3. Read metrics via getMetrics() for monitoring/telemetry
 * 
 * TIMING: <0.5ms per update (simple statistics)
 * MEMORY: ~4KB for history buffers
 */
class SignalQuality {
public:
    SignalQuality();
    ~SignalQuality() = default;

    /**
     * @brief Initialize signal quality analyzer
     * 
     * @return true if initialization successful
     */
    bool init();

    /**
     * @brief Update signal quality metrics with new audio samples
     * 
     * ALGORITHM:
     * 1. Calculate DC offset (mean value)
     * 2. Detect clipping (count samples near saturation)
     * 3. Calculate RMS (root mean square amplitude)
     * 4. Find peak amplitude
     * 5. Estimate SNR (signal vs noise floor)
     * 6. Estimate SPL (sound pressure level)
     * 7. Detect signal presence vs silence
     * 
     * TIMING: <0.5ms per call (128 samples)
     * 
     * @param samples Audio samples (int16_t, typically 128 samples)
     * @param sampleCount Number of samples
     */
    void update(const int16_t* samples, uint16_t sampleCount);

    /**
     * @brief Get current signal quality metrics
     * 
     * @return Reference to metrics structure
     */
    const SignalQualityMetrics& getMetrics() const { return m_metrics; }

    /**
     * @brief Reset signal quality metrics
     * 
     * Use when audio session resets or DSP state clears.
     */
    void reset();

    /**
     * @brief Check if metrics indicate healthy audio signal
     * 
     * HEALTHY SIGNAL CRITERIA:
     * - No clipping (< 5% of samples)
     * - Signal present (RMS > silence threshold)
     * - SNR reasonable (> 20 dB)
     * - DC offset reasonable (< 10% of full scale)
     * 
     * @return true if signal appears healthy
     */
    bool isSignalHealthy() const;

private:
    /**
     * @brief Calculate DC offset (average sample value)
     * 
     * DC offset indicates microphone bias or ADC offset.
     * SPH0645 PDM mics may have small DC component.
     * 
     * @param samples Audio samples
     * @param count Number of samples
     * @return DC offset value
     */
    float calculateDCOffset(const int16_t* samples, uint16_t count);

    /**
     * @brief Detect clipping (samples near saturation)
     * 
     * Clipping occurs when signal exceeds ADC range.
     * Indicates mic gain too high or loud environment.
     * 
     * @param samples Audio samples
     * @param count Number of samples
     * @return Number of clipping samples
     */
    uint16_t detectClipping(const int16_t* samples, uint16_t count);

    /**
     * @brief Calculate RMS (Root Mean Square) amplitude
     * 
     * RMS is a better measure of signal "energy" than simple average.
     * Used for SNR and SPL calculations.
     * 
     * @param samples Audio samples
     * @param count Number of samples
     * @return RMS amplitude
     */
    float calculateRMS(const int16_t* samples, uint16_t count);

    /**
     * @brief Find peak amplitude (max absolute value)
     * 
     * Peak indicates dynamic range and potential clipping.
     * Crest factor = Peak / RMS indicates signal dynamics.
     * 
     * @param samples Audio samples
     * @param count Number of samples
     * @return Peak amplitude
     */
    float calculatePeak(const int16_t* samples, uint16_t count);

    /**
     * @brief Estimate Signal-to-Noise Ratio (SNR) in dB
     * 
     * SNR = 20 * log10(RMS / noise_floor)
     * 
     * APPROXIMATION: Assumes noise floor of ~75 LSB (typical SPH0645)
     * For accurate SNR, need silence calibration period.
     * 
     * @param rms RMS amplitude
     * @return Estimated SNR in dB
     */
    float estimateSNR(float rms);

    /**
     * @brief Estimate Sound Pressure Level (SPL) in dB
     * 
     * SPL = 20 * log10(RMS / full_scale) + reference_offset
     * 
     * APPROXIMATION: Relative to full scale, not absolute dBSPL
     * For absolute SPL, need microphone sensitivity calibration.
     * 
     * @param rms RMS amplitude
     * @return Estimated SPL in dBFS (dB relative to full scale)
     */
    float estimateSPL(float rms);

    // State variables
    SignalQualityMetrics m_metrics;     ///< Current metrics
    float m_rmsHistory[QUALITY_HISTORY_LENGTH];  ///< RMS history for smoothing
    uint16_t m_historyIndex;            ///< Circular buffer index
    bool m_initialized;                 ///< Initialization guard
};

} // namespace Audio
} // namespace LightwaveOS
