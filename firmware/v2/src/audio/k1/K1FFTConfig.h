/**
 * @file K1FFTConfig.h
 * @brief FFT Configuration & Pre-computed Windows for K1 Audio Front-End
 *
 * Provides:
 * - Hann window function (512 samples)
 * - FFT configuration constants
 * - Magnitude scaling & normalization
 *
 * @author LightwaveOS Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace lightwaveos {
namespace audio {
namespace k1 {

// ============================================================================
// FFT Configuration
// ============================================================================

/**
 * @brief FFT Configuration Constants
 *
 * Design for 16kHz audio with 512-point real FFT:
 * - Input size: 512 real samples
 * - Output size: 257 complex bins (0 Hz to Nyquist)
 * - Frequency resolution: 16000 / 512 = 31.25 Hz/bin
 * - Nyquist frequency: 8000 Hz
 * - Update rate: ~31.25ms per FFT (512 samples @ 16kHz)
 *
 * Windowing: Hann window to reduce spectral leakage
 */
class K1FFTConfig {
public:
    static constexpr uint16_t FFT_SIZE = 512;           ///< FFT frame size
    static constexpr uint16_t FFT_SIZE_HALF = 256;      ///< Size / 2 (Nyquist)
    static constexpr uint16_t MAGNITUDE_BINS = 256;     ///< Output magnitude bins (DC to Nyquist)
    static constexpr float SAMPLE_RATE = 16000.0f;      ///< Audio sample rate (Hz)
    static constexpr float FREQ_PER_BIN = SAMPLE_RATE / FFT_SIZE;  ///< 31.25 Hz/bin

    // Window function: Hann window (computed at initialization)
    // w[n] = 0.5 * (1 - cos(2π*n / (N-1)))
    // Applied as: windowed[n] = input[n] * window[n]
    // Properties:
    // - Zero at edges (n=0, n=511)
    // - Maximum at center (n=256) = 1.0
    // - Main lobe: 4 bins (good frequency resolution)
    // - Sidelobe attenuation: -32dB (good for spectral leakage suppression)
    //
    // NOTE: Window is computed at class initialization, not included as static data
    // to save ROM space (512 floats = 2KB). Will be stored in instance or thread-local.

    // ========================================================================
    // Magnitude Normalization
    // ========================================================================

    // Reference level: -20dB = 0.1
    // Magnitude scaling: raw_magnitude / (FFT_SIZE * reference_level)
    // Result: normalized magnitude 0.0-1.0+ (can exceed 1.0 at peaks)
    static constexpr float REFERENCE_LEVEL = 0.1f;
    static constexpr float MAGNITUDE_SCALE = 1.0f / (FFT_SIZE * REFERENCE_LEVEL);

    // ========================================================================
    // Frequency Band Boundaries (in bins)
    // ========================================================================

    // Bass band: 20-200 Hz
    // 20 Hz ≈ bin 0.64 → bin 1
    // 200 Hz ≈ bin 6.4 → bin 6
    static constexpr uint16_t BASS_BIN_START = 1;
    static constexpr uint16_t BASS_BIN_END = 6;

    // Rhythm band: 60-600 Hz (for mag_rhythm[24] compatibility)
    // 60 Hz ≈ bin 1.92 → bin 2
    // 600 Hz ≈ bin 19.2 → bin 19
    static constexpr uint16_t RHYTHM_BIN_START = 2;
    static constexpr uint16_t RHYTHM_BIN_END = 19;
    static constexpr uint16_t RHYTHM_BINS = RHYTHM_BIN_END - RHYTHM_BIN_START + 1;  // 18 bins

    // Mid/Harmony band: 200-2000 Hz (for mag_harmony[64] compatibility)
    // 200 Hz ≈ bin 6.4 → bin 6
    // 2000 Hz ≈ bin 64 → bin 64
    static constexpr uint16_t MID_BIN_START = 6;
    static constexpr uint16_t MID_BIN_END = 64;
    static constexpr uint16_t MID_BINS = MID_BIN_END - MID_BIN_START + 1;  // 59 bins

    // High band: 6000-20000 Hz
    // 6000 Hz ≈ bin 192 → bin 192
    // 20000 Hz ≈ bin 640 (exceeds Nyquist) → bin 256 (max)
    static constexpr uint16_t HIGH_BIN_START = 192;
    static constexpr uint16_t HIGH_BIN_END = 256;

    // ========================================================================
    // Spectral Flux Configuration
    // ========================================================================

    // Flux history: Store previous magnitude for delta calculation
    // flux = Σ max(0, current[k] - previous[k])
    static constexpr uint16_t FLUX_HISTORY_SIZE = 40;  // ~1.25s @ 31.25ms/frame

    // Adaptive threshold: median(flux) + sensitivity * stddev(flux)
    static constexpr float FLUX_SENSITIVITY = 1.5f;    // Synesthesia default

    // ========================================================================
    // Utility Methods
    // ========================================================================

    /**
     * @brief Get frequency (Hz) for a given FFT bin
     * @param bin FFT bin index
     * @return Frequency in Hz
     */
    static inline float getBinFrequency(uint16_t bin) {
        return bin * FREQ_PER_BIN;
    }

    /**
     * @brief Get FFT bin for a given frequency (Hz)
     * @param freq Frequency in Hz
     * @return FFT bin index (rounded)
     */
    static inline uint16_t getFrequencyBin(float freq) {
        return static_cast<uint16_t>(freq / FREQ_PER_BIN + 0.5f);
    }

    /**
     * @brief Generate Hann window coefficients
     * @param window Output array (must be FFT_SIZE)
     *
     * Formula: w[n] = 0.5 * (1 - cos(2π*n / (N-1)))
     */
    static inline void generateHannWindow(float window[FFT_SIZE]) {
        const float pi = 3.14159265358979323846f;
        const float scale = 2.0f * pi / (FFT_SIZE - 1);

        for (uint16_t n = 0; n < FFT_SIZE; n++) {
            window[n] = 0.5f * (1.0f - std::cos(scale * n));
        }
    }

    /**
     * @brief Apply Hann window to audio frame
     * @param input Input audio samples (512)
     * @param window Pre-computed Hann window (512)
     * @param output Windowed output (512)
     */
    static inline void applyHannWindow(const float input[FFT_SIZE],
                                      const float window[FFT_SIZE],
                                      float output[FFT_SIZE]) {
        for (uint16_t i = 0; i < FFT_SIZE; i++) {
            output[i] = input[i] * window[i];
        }
    }

    /**
     * @brief Apply Hann window in-place to audio frame
     * @param samples Audio samples to window (512)
     * @param window Pre-computed Hann window (512)
     */
    static inline void applyHannWindowInPlace(float samples[FFT_SIZE],
                                             const float window[FFT_SIZE]) {
        for (uint16_t i = 0; i < FFT_SIZE; i++) {
            samples[i] *= window[i];
        }
    }

private:
    K1FFTConfig() = delete;  // Static utility class, cannot instantiate
};

}  // namespace k1
}  // namespace audio
}  // namespace lightwaveos
