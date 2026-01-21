/**
 * @file SignalQuality.cpp
 * @brief Audio signal quality metrics implementation
 * 
 * COMMERCIAL DEPLOYMENT REQUIREMENT
 * 
 * Provides real-time observability into audio pipeline health.
 * Essential for debugging, tuning, and deployment validation.
 * 
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "SignalQuality.h"
#include <cstring>  // For memset
#include <cmath>    // For log10f, sqrtf

namespace LightwaveOS {
namespace Audio {

// ============================================================================
// MATHEMATICAL CONSTANTS
// ============================================================================

constexpr float LOG10_CONSTANT = 2.302585093f;  // ln(10) for log10 conversion

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

SignalQuality::SignalQuality()
    : m_historyIndex(0)
    , m_initialized(false)
{
    // Zero-initialize
    memset(&m_metrics, 0, sizeof(m_metrics));
    memset(m_rmsHistory, 0, sizeof(m_rmsHistory));
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool SignalQuality::init() {
    if (m_initialized) {
        return true;  // Already initialized
    }

    reset();

    m_initialized = true;
    return true;
}

void SignalQuality::reset() {
    // Zero all state
    memset(&m_metrics, 0, sizeof(m_metrics));
    memset(m_rmsHistory, 0, sizeof(m_rmsHistory));
    m_historyIndex = 0;
}

// ============================================================================
// SIGNAL QUALITY UPDATE (MAIN ENTRY POINT)
// ============================================================================

void SignalQuality::update(const int16_t* samples, uint16_t sampleCount) {
    /**
     * Calculate all signal quality metrics
     * 
     * TIMING: <0.5ms for 128 samples @ 240MHz
     * - DC offset: O(n) single pass
     * - Clipping: O(n) single pass (can combine with DC)
     * - RMS: O(n) single pass
     * - Peak: O(n) single pass (can combine with RMS)
     * - SNR/SPL: O(1) from RMS
     */
    
    // Increment hop counter
    m_metrics.hopCount++;
    
    // Calculate DC offset (raw and after any correction)
    m_metrics.dcOffset = calculateDCOffset(samples, sampleCount);
    m_metrics.dcOffsetRaw = m_metrics.dcOffset;  // TODO: Store pre-correction value
    
    // Detect clipping
    m_metrics.clippingCount = detectClipping(samples, sampleCount);
    m_metrics.clippingPercent = (m_metrics.clippingCount * 100.0f) / sampleCount;
    m_metrics.isClipping = (m_metrics.clippingPercent > 5.0f);
    
    // Calculate RMS and peak
    m_metrics.rms = calculateRMS(samples, sampleCount);
    m_metrics.peak = calculatePeak(samples, sampleCount);
    
    // Crest factor (dynamic range indicator)
    m_metrics.crestFactor = (m_metrics.rms > 0.0f) 
        ? (m_metrics.peak / m_metrics.rms) 
        : 0.0f;
    
    // Estimate SNR and SPL
    m_metrics.snrEstimate = estimateSNR(m_metrics.rms);
    m_metrics.splEstimate = estimateSPL(m_metrics.rms);
    
    // Detect signal presence
    m_metrics.signalPresent = (m_metrics.rms > SILENCE_THRESHOLD);
    
    // Track silent hops
    if (m_metrics.signalPresent) {
        m_metrics.silentHopCount = 0;
    } else {
        m_metrics.silentHopCount++;
    }
    
    // Store RMS in history for smoothing
    m_rmsHistory[m_historyIndex] = m_metrics.rms;
    m_historyIndex = (m_historyIndex + 1) % QUALITY_HISTORY_LENGTH;
}

// ============================================================================
// DC OFFSET CALCULATION
// ============================================================================

float SignalQuality::calculateDCOffset(const int16_t* samples, uint16_t count) {
    /**
     * Calculate DC offset (average sample value)
     * 
     * DC offset indicates microphone bias or ADC offset.
     * SPH0645 PDM microphones may have small DC component.
     * 
     * FORMULA: DC = sum(samples) / count
     * 
     * HEALTHY RANGE: -1000 to +1000 (< 3% of full scale)
     * CONCERN: > 3000 indicates possible bias issue
     */
    
    int64_t sum = 0;
    
    for (uint16_t i = 0; i < count; i++) {
        sum += samples[i];
    }
    
    return static_cast<float>(sum) / count;
}

// ============================================================================
// CLIPPING DETECTION
// ============================================================================

uint16_t SignalQuality::detectClipping(const int16_t* samples, uint16_t count) {
    /**
     * Detect clipping (samples near saturation)
     * 
     * Clipping occurs when signal exceeds ADC range.
     * Indicates:
     * - Mic gain too high
     * - Very loud environment
     * - ADC input stage saturation
     * 
     * THRESHOLD: 31000 (95% of 32767 max)
     * 
     * HEALTHY: 0 clipping samples
     * CONCERN: > 5% of samples clipping
     */
    
    uint16_t clippingCount = 0;
    
    for (uint16_t i = 0; i < count; i++) {
        int16_t absValue = (samples[i] < 0) ? -samples[i] : samples[i];
        
        if (absValue > CLIPPING_THRESHOLD) {
            clippingCount++;
        }
    }
    
    return clippingCount;
}

// ============================================================================
// RMS CALCULATION
// ============================================================================

float SignalQuality::calculateRMS(const int16_t* samples, uint16_t count) {
    /**
     * Calculate RMS (Root Mean Square) amplitude
     * 
     * RMS is a better measure of signal "energy" than simple average.
     * Represents the equivalent DC level that would dissipate the same power.
     * 
     * FORMULA: RMS = sqrt(sum(sample²) / count)
     * 
     * WHY RMS:
     * - Accounts for both positive and negative samples
     * - More representative of perceived loudness
     * - Standard metric for audio level metering
     * 
     * RANGE: [0, 32767] where 32767 = full scale sine wave
     */
    
    int64_t sumSquares = 0;
    
    for (uint16_t i = 0; i < count; i++) {
        int32_t sample = samples[i];
        sumSquares += sample * sample;
    }
    
    float meanSquare = static_cast<float>(sumSquares) / count;
    return sqrtf(meanSquare);
}

// ============================================================================
// PEAK CALCULATION
// ============================================================================

float SignalQuality::calculatePeak(const int16_t* samples, uint16_t count) {
    /**
     * Find peak amplitude (max absolute value)
     * 
     * Peak indicates:
     * - Maximum signal excursion
     * - Headroom before clipping
     * - Dynamic range (via crest factor)
     * 
     * CREST FACTOR = Peak / RMS
     * - Music: 12-20 dB typical (3-10 ratio)
     * - Speech: 10-15 dB typical (3-6 ratio)
     * - Noise: 3-6 dB typical (1.4-2 ratio)
     * 
     * High crest factor = dynamic content (music, transients)
     * Low crest factor = constant level (noise, tone)
     */
    
    int16_t peak = 0;
    
    for (uint16_t i = 0; i < count; i++) {
        int16_t absValue = (samples[i] < 0) ? -samples[i] : samples[i];
        
        if (absValue > peak) {
            peak = absValue;
        }
    }
    
    return static_cast<float>(peak);
}

// ============================================================================
// SNR ESTIMATION
// ============================================================================

float SignalQuality::estimateSNR(float rms) {
    /**
     * Estimate Signal-to-Noise Ratio (SNR) in dB
     * 
     * SNR = 20 * log10(signal / noise)
     * 
     * APPROXIMATION: Assumes noise floor of ~75 LSB
     * - Typical SPH0645 + ESP32 I2S noise floor
     * - Measured during silence periods
     * 
     * ACCURATE SNR REQUIRES:
     * - Silence calibration period to measure actual noise floor
     * - Multiple averaging windows
     * - Spectral analysis (frequency-dependent noise)
     * 
     * INTERPRETATION:
     * - SNR > 40 dB: Excellent signal quality
     * - SNR 30-40 dB: Good signal quality
     * - SNR 20-30 dB: Acceptable signal quality
     * - SNR < 20 dB: Poor signal quality (mostly noise)
     */
    
    if (rms < NOISE_FLOOR_ESTIMATE) {
        return 0.0f;  // Signal below noise floor
    }
    
    float snr = 20.0f * log10f(rms / NOISE_FLOOR_ESTIMATE);
    return snr;
}

// ============================================================================
// SPL ESTIMATION
// ============================================================================

float SignalQuality::estimateSPL(float rms) {
    /**
     * Estimate Sound Pressure Level (SPL) in dB
     * 
     * SPL = 20 * log10(rms / full_scale_rms)
     * 
     * IMPORTANT: This is dBFS (dB relative to Full Scale), NOT absolute dBSPL!
     * 
     * For absolute dBSPL, we would need:
     * - Microphone sensitivity spec (dBV/Pa or mV/Pa)
     * - ADC gain and reference voltage
     * - Acoustic calibration with known SPL source
     * 
     * APPROXIMATION RANGE:
     * - 0 dBFS = full scale (32767 RMS)
     * - -6 dBFS = 50% amplitude (16384 RMS)
     * - -20 dBFS = 10% amplitude (3277 RMS)
     * - -40 dBFS = 1% amplitude (328 RMS)
     * - -60 dBFS = 0.1% amplitude (33 RMS) - near noise floor
     * 
     * TYPICAL MUSIC/SPEECH:
     * - Peaks: -6 to 0 dBFS
     * - Average: -20 to -10 dBFS
     * - Quiet: -40 to -30 dBFS
     */
    
    if (rms < 1.0f) {
        return -100.0f;  // Effectively silence
    }
    
    // Full scale RMS for sine wave = 32767 / sqrt(2) ≈ 23170
    // For general audio, use 32767 as reference (square wave = worst case)
    float spl = 20.0f * log10f(rms / 32767.0f);
    return spl;
}

// ============================================================================
// SIGNAL HEALTH CHECK
// ============================================================================

bool SignalQuality::isSignalHealthy() const {
    /**
     * Check if signal metrics indicate healthy audio
     * 
     * HEALTHY SIGNAL CRITERIA:
     * 1. No clipping (< 5% of samples)
     * 2. Signal present (RMS > silence threshold)
     * 3. SNR reasonable (> 20 dB)
     * 4. DC offset reasonable (< 10% of full scale = 3277)
     * 
     * FAILS IF:
     * - Clipping detected (distortion)
     * - No signal / dead mic
     * - Very low SNR (noisy environment or mic issue)
     * - Large DC offset (bias/calibration issue)
     */
    
    bool noClipping = !m_metrics.isClipping;
    bool hasSignal = m_metrics.signalPresent;
    bool goodSNR = (m_metrics.snrEstimate > 20.0f);
    bool dcOkay = (fabsf(m_metrics.dcOffset) < 3277.0f);  // < 10% of full scale
    
    return noClipping && hasSignal && goodSNR && dcOkay;
}

} // namespace Audio
} // namespace LightwaveOS
