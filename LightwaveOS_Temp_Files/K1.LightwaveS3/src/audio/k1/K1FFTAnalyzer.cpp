/**
 * @file K1FFTAnalyzer.cpp
 * @brief Implementation of K1FFT Analyzer (STUB - Phase 0 placeholder)
 *
 * TODO: Integrate actual KissFFT from FastLED third_party cq_kernel
 * This is a placeholder to allow compilation while TempoTracker (Phases 3-5)
 * is validated on hardware.
 */

#include "K1FFTAnalyzer.h"
#include <cmath>
#include <cstring>

namespace lightwaveos {
namespace audio {
namespace k1 {

K1FFTAnalyzer::K1FFTAnalyzer()
    : m_fftCfg(nullptr),
      m_initialized(false) {
    reset();
}

K1FFTAnalyzer::~K1FFTAnalyzer() {
    destroy();
}

bool K1FFTAnalyzer::init() {
    if (m_initialized) {
        return true;
    }

    // Allocate KissFFT context for 512-point real FFT
    m_fftCfg = kiss_fftr_alloc(K1FFTConfig::FFT_SIZE, 0, nullptr, nullptr);
    if (m_fftCfg == nullptr) {
        return false;  // Allocation failed
    }

    // Pre-compute Hann window
    K1FFTConfig::generateHannWindow(m_hannWindow);

    // Clear buffers
    reset();

    m_initialized = true;
    return true;
}

void K1FFTAnalyzer::destroy() {
    if (m_fftCfg != nullptr) {
        kiss_fft_free(m_fftCfg);
        m_fftCfg = nullptr;
    }
    m_initialized = false;
}

bool K1FFTAnalyzer::processFrame(const float input[K1FFTConfig::FFT_SIZE]) {
    if (!m_initialized || m_fftCfg == nullptr) {
        return false;
    }

    // Apply Hann window to input (produces float output)
    K1FFTConfig::applyHannWindow(input, m_hannWindow, m_fftInputFloat);

    // Convert windowed float samples to int16_t for KissFFT
    for (size_t i = 0; i < K1FFTConfig::FFT_SIZE; i++) {
        float val = m_fftInputFloat[i] * 32767.0f;  // Scale to int16_t range
        m_fftInput[i] = static_cast<int16_t>(val);
    }

    // Perform real FFT: input is 512 int16_t samples, output is 257 complex numbers
    kiss_fftr(reinterpret_cast<kiss_fftr_cfg>(m_fftCfg), m_fftInput, m_fftOutput);

    // Extract magnitude spectrum
    extractMagnitude(m_fftOutput, m_magnitude);

    return true;
}

void K1FFTAnalyzer::extractMagnitude(const kiss_fft_cpx fftOut[],
                                     float magnitude[K1FFTConfig::MAGNITUDE_BINS]) {
    // Extract magnitude from complex FFT output
    // magnitude = sqrt(real^2 + imag^2)
    const uint16_t numBins = K1FFTConfig::FFT_SIZE / 2;  // 256

    for (uint16_t k = 0; k < numBins; k++) {
        float real = fftOut[k].r;
        float imag = fftOut[k].i;

        // Compute magnitude and normalize by FFT size (standard scaling)
        float mag = std::sqrt(real * real + imag * imag);
        magnitude[k] = mag / static_cast<float>(K1FFTConfig::FFT_SIZE);
    }
}

}  // namespace k1
}  // namespace audio
}  // namespace lightwaveos
