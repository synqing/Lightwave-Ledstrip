/**
 * @file K1FFTAnalyzer.h
 * @brief Real FFT Analyzer for K1 Audio Front-End (using KissFFT)
 *
 * Wraps KissFFT library to perform 512-point real FFT at 16kHz.
 * Provides magnitude spectrum output with normalization.
 *
 * Features:
 * - Real FFT (512 input samples → 256 magnitude bins)
 * - Hann windowing to reduce spectral leakage
 * - Magnitude normalization (0.0-1.0+)
 * - Memory pre-allocation (no allocations in hot path)
 *
 * @author LightwaveOS Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>
#include "K1FFTConfig.h"

// KissFFT: Use FastLED's embedded implementation
// TODO: Integrate KissFFT from FastLED when API is finalized
// For now, using placeholder struct to verify compilation

// Placeholder for KissFFT configuration (from cq_kernel)
typedef void* kiss_fftr_cfg;
typedef struct {
    float r;
    float i;
} kiss_fft_cpx;

namespace lightwaveos {
namespace audio {
namespace k1 {

/**
 * @brief Real FFT Analyzer
 *
 * Processes 512-sample audio frames through KissFFT to produce
 * frequency-domain magnitude spectrum.
 *
 * Thread Safety: Not thread-safe. Should be called from single thread only.
 */
class K1FFTAnalyzer {
public:
    /**
     * @brief Constructor
     */
    K1FFTAnalyzer();

    /**
     * @brief Destructor
     */
    ~K1FFTAnalyzer();

    /**
     * @brief Initialize FFT analyzer
     *
     * Allocates KissFFT context and pre-computes Hann window.
     *
     * @return true if successful, false if allocation failed
     */
    bool init();

    /**
     * @brief Release resources
     *
     * Called automatically by destructor.
     */
    void destroy();

    /**
     * @brief Process one frame of audio samples
     *
     * Applies Hann window, performs FFT, extracts magnitudes.
     *
     * @param input Raw audio samples (512 floats)
     * @return true if processing successful
     */
    bool processFrame(const float input[K1FFTConfig::FFT_SIZE]);

    /**
     * @brief Get magnitude spectrum
     *
     * Returns normalized magnitude values (0.0-1.0+).
     *
     * @return Pointer to magnitude array (256 bins)
     */
    const float* getMagnitude() const {
        return m_magnitude;
    }

    /**
     * @brief Get magnitude for specific bin
     *
     * @param bin Bin index (0-255)
     * @return Magnitude value (0.0-1.0+)
     */
    float getMagnitudeBin(uint16_t bin) const {
        if (bin >= K1FFTConfig::MAGNITUDE_BINS) {
            return 0.0f;
        }
        return m_magnitude[bin];
    }

    /**
     * @brief Get magnitude in frequency band
     *
     * Sums magnitude across specified bin range.
     *
     * @param startBin Start bin (inclusive)
     * @param endBin End bin (inclusive)
     * @return Sum of magnitudes
     */
    float getMagnitudeRange(uint16_t startBin, uint16_t endBin) const {
        float sum = 0.0f;
        if (startBin >= K1FFTConfig::MAGNITUDE_BINS) {
            return 0.0f;
        }
        endBin = std::min(endBin, (uint16_t)(K1FFTConfig::MAGNITUDE_BINS - 1));

        for (uint16_t i = startBin; i <= endBin; i++) {
            sum += m_magnitude[i];
        }
        return sum;
    }

    /**
     * @brief Reset analyzer state
     */
    void reset() {
        std::memset(m_magnitude, 0, sizeof(m_magnitude));
        std::memset(m_fftInput, 0, sizeof(m_fftInput));
    }

    /**
     * @brief Check if initialized
     * @return true if init() was successful
     */
    bool isInitialized() const {
        return m_initialized && m_fftCfg != nullptr;
    }

private:
    // KissFFT context
    kiss_fftr_cfg m_fftCfg;  ///< FFT configuration (allocated in init())

    // Pre-allocated buffers
    float m_fftInput[K1FFTConfig::FFT_SIZE];      ///< Windowed input for FFT
    kiss_fft_cpx m_fftOutput[K1FFTConfig::FFT_SIZE / 2 + 1];  ///< FFT output (complex)
    float m_magnitude[K1FFTConfig::MAGNITUDE_BINS];  ///< Magnitude spectrum
    float m_hannWindow[K1FFTConfig::FFT_SIZE];    ///< Pre-computed Hann window

    bool m_initialized;  ///< Initialization flag

    /**
     * @brief Extract magnitude from complex FFT output
     *
     * @param fftOut Complex FFT output array
     * @param magnitude Output magnitude array
     */
    void extractMagnitude(const kiss_fft_cpx fftOut[],
                         float magnitude[K1FFTConfig::MAGNITUDE_BINS]);
};

}  // namespace k1
}  // namespace audio
}  // namespace lightwaveos
