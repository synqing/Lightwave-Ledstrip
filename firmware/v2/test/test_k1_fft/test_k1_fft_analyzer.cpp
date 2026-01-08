/**
 * @file test_k1_fft_analyzer.cpp
 * @brief Unit tests for K1FFT Analyzer (KissFFT wrapper)
 *
 * Validates:
 * - FFT accuracy with known test signals
 * - Hann window properties
 * - Magnitude extraction and normalization
 * - Spectral flux calculation
 */

#include "unity.h"
#include "K1FFTConfig.h"
#include "K1FFTAnalyzer.h"
#include "SpectralFlux.h"
#include "FrequencyBandExtractor.h"
#include <cmath>
#include <cstring>

using namespace lightwaveos::audio::k1;

// =============================================================================
// Test Fixtures
// =============================================================================

void setUp(void) {
    // Run before each test
}

void tearDown(void) {
    // Run after each test
}

// =============================================================================
// K1FFTAnalyzer Tests
// =============================================================================

void test_k1fft_analyzer_init(void) {
    K1FFTAnalyzer analyzer;
    TEST_ASSERT_FALSE(analyzer.isInitialized());

    bool result = analyzer.init();
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(analyzer.isInitialized());
}

void test_k1fft_analyzer_process_zero_signal(void) {
    K1FFTAnalyzer analyzer;
    analyzer.init();

    // Create zero input (silence)
    float input[K1FFTConfig::FFT_SIZE];
    std::memset(input, 0, sizeof(input));

    // Process frame
    bool result = analyzer.processFrame(input);
    TEST_ASSERT_TRUE(result);

    // Verify output is (nearly) zero
    const float* magnitude = analyzer.getMagnitude();
    float totalMagnitude = 0.0f;
    for (uint16_t k = 0; k < K1FFTConfig::MAGNITUDE_BINS; k++) {
        totalMagnitude += magnitude[k];
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, totalMagnitude);
}

void test_k1fft_analyzer_sine_wave_detection(void) {
    K1FFTAnalyzer analyzer;
    analyzer.init();

    // Generate 1000 Hz sine wave (512 samples at 16kHz = 32ms)
    // 1000 Hz ≈ bin 32 (31.25 Hz/bin)
    float input[K1FFTConfig::FFT_SIZE];
    const float frequency = 1000.0f;  // Hz
    const float sampleRate = K1FFTConfig::SAMPLE_RATE;
    const float amplitude = 0.5f;  // 0.5 peak amplitude

    for (uint16_t n = 0; n < K1FFTConfig::FFT_SIZE; n++) {
        float phase = 2.0f * 3.14159265358979f * frequency * n / sampleRate;
        input[n] = amplitude * std::sin(phase);
    }

    // Process frame
    bool result = analyzer.processFrame(input);
    TEST_ASSERT_TRUE(result);

    // Find peak bin
    const float* magnitude = analyzer.getMagnitude();
    uint16_t peakBin = 0;
    float peakMagnitude = 0.0f;
    for (uint16_t k = 1; k < K1FFTConfig::MAGNITUDE_BINS - 1; k++) {
        if (magnitude[k] > peakMagnitude) {
            peakMagnitude = magnitude[k];
            peakBin = k;
        }
    }

    // Verify peak is near bin 32 (1000 Hz)
    uint16_t expectedBin = K1FFTConfig::getFrequencyBin(frequency);
    TEST_ASSERT_INT_WITHIN(2, expectedBin, peakBin);  // Allow ±2 bins
}

void test_k1fft_analyzer_magnitude_range(void) {
    K1FFTAnalyzer analyzer;
    analyzer.init();

    // Generate white noise (all frequencies)
    float input[K1FFTConfig::FFT_SIZE];
    for (uint16_t n = 0; n < K1FFTConfig::FFT_SIZE; n++) {
        // Pseudo-random using LCG
        static uint32_t seed = 12345;
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        input[n] = (seed / 1073741824.0f) - 0.5f;  // -0.5 to +0.5
    }

    // Process frame
    bool result = analyzer.processFrame(input);
    TEST_ASSERT_TRUE(result);

    // Verify magnitude values are reasonable (0.0 to ~2.0)
    const float* magnitude = analyzer.getMagnitude();
    for (uint16_t k = 0; k < K1FFTConfig::MAGNITUDE_BINS; k++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, magnitude[k]);  // Rough check
        TEST_ASSERT_TRUE(magnitude[k] >= 0.0f);
        TEST_ASSERT_TRUE(magnitude[k] <= 5.0f);  // Allow headroom for peaks
    }
}

void test_k1fft_analyzer_get_magnitude_bin(void) {
    K1FFTAnalyzer analyzer;
    analyzer.init();

    float input[K1FFTConfig::FFT_SIZE];
    std::memset(input, 0, sizeof(input));
    analyzer.processFrame(input);

    // Test single bin accessor
    float bin0 = analyzer.getMagnitudeBin(0);
    TEST_ASSERT_TRUE(bin0 >= 0.0f);

    // Test out-of-bounds accessor (should return 0)
    float binOOB = analyzer.getMagnitudeBin(500);  // Out of bounds
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, binOOB);
}

void test_k1fft_analyzer_magnitude_range_sum(void) {
    K1FFTAnalyzer analyzer;
    analyzer.init();

    // Generate 1000 Hz sine
    float input[K1FFTConfig::FFT_SIZE];
    for (uint16_t n = 0; n < K1FFTConfig::FFT_SIZE; n++) {
        float phase = 2.0f * 3.14159265358979f * 1000.0f * n / K1FFTConfig::SAMPLE_RATE;
        input[n] = 0.5f * std::sin(phase);
    }

    analyzer.processFrame(input);

    // Test getMagnitudeRange
    float bandSum = analyzer.getMagnitudeRange(30, 35);
    TEST_ASSERT_TRUE(bandSum > 0.0f);
    TEST_ASSERT_TRUE(bandSum <= 10.0f);
}

// =============================================================================
// Hann Window Tests
// =============================================================================

void test_hann_window_generation(void) {
    float window[K1FFTConfig::FFT_SIZE];
    K1FFTConfig::generateHannWindow(window);

    // Verify window is zero at edges
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, window[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, window[K1FFTConfig::FFT_SIZE - 1]);

    // Verify window is ~1.0 at center (slight offset due to formula)
    float centerValue = window[K1FFTConfig::FFT_SIZE / 2];
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, centerValue);

    // Verify all values are in [0, 1]
    for (uint16_t n = 0; n < K1FFTConfig::FFT_SIZE; n++) {
        TEST_ASSERT_TRUE(window[n] >= -1e-6f);
        TEST_ASSERT_TRUE(window[n] <= 1.0f + 1e-6f);
    }
}

void test_hann_window_symmetry(void) {
    float window[K1FFTConfig::FFT_SIZE];
    K1FFTConfig::generateHannWindow(window);

    // Hann window should be symmetric
    for (uint16_t n = 0; n < K1FFTConfig::FFT_SIZE / 2; n++) {
        uint16_t mirror = K1FFTConfig::FFT_SIZE - 1 - n;
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, window[n], window[mirror]);
    }
}

void test_hann_window_application(void) {
    float window[K1FFTConfig::FFT_SIZE];
    K1FFTConfig::generateHannWindow(window);

    // Create test input (all 1.0)
    float input[K1FFTConfig::FFT_SIZE];
    for (uint16_t i = 0; i < K1FFTConfig::FFT_SIZE; i++) {
        input[i] = 1.0f;
    }

    // Apply window
    float output[K1FFTConfig::FFT_SIZE];
    K1FFTConfig::applyHannWindow(input, window, output);

    // Output should equal window (since input = 1.0)
    for (uint16_t i = 0; i < K1FFTConfig::FFT_SIZE; i++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, window[i], output[i]);
    }

    // Output edges should be near zero
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, output[K1FFTConfig::FFT_SIZE - 1]);
}

void test_hann_window_inplace_application(void) {
    float window[K1FFTConfig::FFT_SIZE];
    K1FFTConfig::generateHannWindow(window);

    // Create test input
    float samples[K1FFTConfig::FFT_SIZE];
    for (uint16_t i = 0; i < K1FFTConfig::FFT_SIZE; i++) {
        samples[i] = 1.0f;
    }

    // Apply window in-place
    K1FFTConfig::applyHannWindowInPlace(samples, window);

    // Should equal window
    for (uint16_t i = 0; i < K1FFTConfig::FFT_SIZE; i++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, window[i], samples[i]);
    }
}

// =============================================================================
// SpectralFlux Tests
// =============================================================================

void test_spectral_flux_init(void) {
    SpectralFlux flux;
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, flux.getFlux());
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, flux.getPreviousFlux());
}

void test_spectral_flux_zero_signal(void) {
    SpectralFlux flux;

    // Process zero magnitude
    float magnitude[K1FFTConfig::MAGNITUDE_BINS];
    std::memset(magnitude, 0, sizeof(magnitude));

    float result = flux.process(magnitude);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, result);

    // Process again (should still be zero)
    result = flux.process(magnitude);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, result);
}

void test_spectral_flux_positive_change(void) {
    SpectralFlux flux;

    // First frame: all zeros
    float magnitude1[K1FFTConfig::MAGNITUDE_BINS];
    std::memset(magnitude1, 0, sizeof(magnitude1));
    flux.process(magnitude1);

    // Second frame: all 1.0 (positive change everywhere)
    float magnitude2[K1FFTConfig::MAGNITUDE_BINS];
    for (uint16_t k = 0; k < K1FFTConfig::MAGNITUDE_BINS; k++) {
        magnitude2[k] = 1.0f;
    }

    float result = flux.process(magnitude2);
    float expected = K1FFTConfig::MAGNITUDE_BINS * 1.0f;
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, expected, result);
}

void test_spectral_flux_ignores_decreases(void) {
    SpectralFlux flux;

    // First frame: all 1.0
    float magnitude1[K1FFTConfig::MAGNITUDE_BINS];
    for (uint16_t k = 0; k < K1FFTConfig::MAGNITUDE_BINS; k++) {
        magnitude1[k] = 1.0f;
    }
    flux.process(magnitude1);

    // Second frame: all 0.5 (decrease - should be ignored)
    float magnitude2[K1FFTConfig::MAGNITUDE_BINS];
    for (uint16_t k = 0; k < K1FFTConfig::MAGNITUDE_BINS; k++) {
        magnitude2[k] = 0.5f;
    }

    float result = flux.process(magnitude2);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, result);  // No positive changes = zero flux
}

void test_spectral_flux_history(void) {
    SpectralFlux flux;

    // Process several frames with increasing flux
    for (uint16_t frame = 0; frame < 10; frame++) {
        float magnitude[K1FFTConfig::MAGNITUDE_BINS];
        float value = (frame + 1) * 0.1f;
        for (uint16_t k = 0; k < K1FFTConfig::MAGNITUDE_BINS; k++) {
            magnitude[k] = value;
        }
        flux.process(magnitude);
    }

    // Verify history is available
    const float* history = flux.getFluxHistory();
    TEST_ASSERT_NOT_NULL(history);
    TEST_ASSERT_EQUAL(SpectralFlux::FLUX_HISTORY_SIZE, flux.getFluxHistorySize());
}

// =============================================================================
// FrequencyBandExtractor Tests
// =============================================================================

void test_frequency_band_extractor_bass_energy(void) {
    float magnitude[K1FFTConfig::MAGNITUDE_BINS];
    std::memset(magnitude, 0, sizeof(magnitude));

    // Set bass band to 1.0
    for (uint16_t k = K1FFTConfig::BASS_BIN_START; k <= K1FFTConfig::BASS_BIN_END; k++) {
        magnitude[k] = 1.0f;
    }

    float energy = FrequencyBandExtractor::getBassEnergy(magnitude);
    uint16_t expectedBins = K1FFTConfig::BASS_BIN_END - K1FFTConfig::BASS_BIN_START + 1;
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, (float)expectedBins, energy);
}

void test_frequency_band_extractor_rhythm_energy(void) {
    float magnitude[K1FFTConfig::MAGNITUDE_BINS];
    std::memset(magnitude, 0, sizeof(magnitude));

    // Set rhythm band to 1.0
    for (uint16_t k = K1FFTConfig::RHYTHM_BIN_START; k <= K1FFTConfig::RHYTHM_BIN_END; k++) {
        magnitude[k] = 1.0f;
    }

    float energy = FrequencyBandExtractor::getRhythmEnergy(magnitude);
    uint16_t expectedBins = K1FFTConfig::RHYTHM_BIN_END - K1FFTConfig::RHYTHM_BIN_START + 1;
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, (float)expectedBins, energy);
}

void test_frequency_band_extractor_mid_energy(void) {
    float magnitude[K1FFTConfig::MAGNITUDE_BINS];
    std::memset(magnitude, 0, sizeof(magnitude));

    // Set mid band to 1.0
    for (uint16_t k = K1FFTConfig::MID_BIN_START; k <= K1FFTConfig::MID_BIN_END; k++) {
        magnitude[k] = 1.0f;
    }

    float energy = FrequencyBandExtractor::getMidEnergy(magnitude);
    uint16_t expectedBins = K1FFTConfig::MID_BIN_END - K1FFTConfig::MID_BIN_START + 1;
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, (float)expectedBins, energy);
}

void test_frequency_band_extractor_total_energy(void) {
    float magnitude[K1FFTConfig::MAGNITUDE_BINS];
    for (uint16_t k = 0; k < K1FFTConfig::MAGNITUDE_BINS; k++) {
        magnitude[k] = 1.0f;
    }

    float energy = FrequencyBandExtractor::getTotalEnergy(magnitude);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, (float)K1FFTConfig::MAGNITUDE_BINS, energy);
}

void test_frequency_band_extractor_map_rhythm(void) {
    float magnitude[K1FFTConfig::MAGNITUDE_BINS];
    std::memset(magnitude, 0, sizeof(magnitude));

    // Set rhythm band to 1.0
    for (uint16_t k = K1FFTConfig::RHYTHM_BIN_START; k <= K1FFTConfig::RHYTHM_BIN_END; k++) {
        magnitude[k] = 1.0f;
    }

    float rhythmArray[24];
    FrequencyBandExtractor::mapToRhythmArray(magnitude, rhythmArray);

    // All array elements should be positive (non-zero due to mapping)
    for (uint16_t i = 0; i < 24; i++) {
        TEST_ASSERT_TRUE(rhythmArray[i] >= 0.0f);
    }

    // Sum should be close to total rhythm energy
    float sum = 0.0f;
    for (uint16_t i = 0; i < 24; i++) {
        sum += rhythmArray[i];
    }
    uint16_t rhythmBins = K1FFTConfig::RHYTHM_BIN_END - K1FFTConfig::RHYTHM_BIN_START + 1;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, (float)rhythmBins, sum);
}

void test_frequency_band_extractor_map_harmony(void) {
    float magnitude[K1FFTConfig::MAGNITUDE_BINS];
    std::memset(magnitude, 0, sizeof(magnitude));

    // Set mid band to 1.0
    for (uint16_t k = K1FFTConfig::MID_BIN_START; k <= K1FFTConfig::MID_BIN_END; k++) {
        magnitude[k] = 1.0f;
    }

    float harmonyArray[64];
    FrequencyBandExtractor::mapToHarmonyArray(magnitude, harmonyArray);

    // All array elements should be positive
    for (uint16_t i = 0; i < 64; i++) {
        TEST_ASSERT_TRUE(harmonyArray[i] >= 0.0f);
    }

    // Sum should be close to total mid energy
    float sum = 0.0f;
    for (uint16_t i = 0; i < 64; i++) {
        sum += harmonyArray[i];
    }
    uint16_t midBins = K1FFTConfig::MID_BIN_END - K1FFTConfig::MID_BIN_START + 1;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, (float)midBins, sum);
}
