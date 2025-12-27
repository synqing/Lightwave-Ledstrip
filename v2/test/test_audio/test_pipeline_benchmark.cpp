/**
 * @file test_pipeline_benchmark.cpp
 * @brief Audio pipeline A/B benchmark test suite
 *
 * Implements the validation framework from the Sensory Bridge comparative
 * analysis. Tests different audio pipeline configurations and reports
 * quantitative metrics for comparison.
 *
 * Build: pio test -e native_test -f test_pipeline_benchmark
 * Run: .pio/build/native_test/program
 */

#define FEATURE_AUDIO_SYNC 1

#include <unity.h>
#include <cstdio>
#include <cstring>

#include "TestSignalGenerator.h"
#include "AudioPipelineBenchmark.h"
#include "../../src/audio/GoertzelAnalyzer.h"
#include "../../src/audio/AudioTuning.h"

using namespace lightwaveos::audio;
using namespace lightwaveos::audio::test;

// Global test fixtures
static TestSignalGenerator g_signalGen;
static AudioPipelineBenchmark g_benchmark;
static GoertzelAnalyzer g_analyzer;

// Test buffers
static int16_t g_testBuffer[512];
static float g_bands[8];

void setUp(void) {
    g_signalGen.seed(0x12345678);  // Reproducible random
    g_benchmark.reset();
    g_analyzer.reset();
}

void tearDown(void) {
}

// =============================================================================
// SPECTRAL ACCURACY TESTS
// =============================================================================

/**
 * @brief Test Goertzel frequency detection for each target band
 *
 * Generates pure sine waves at each target frequency and verifies
 * the correct band has maximum magnitude.
 */
void test_spectral_accuracy_per_band(void) {
    printf("\n  Testing spectral accuracy for 8 bands...\n");

    for (uint8_t band = 0; band < 8; ++band) {
        g_signalGen.generateGoertzelTarget(g_testBuffer, 512, band, 0.5f);

        g_analyzer.reset();
        g_analyzer.accumulate(g_testBuffer, 512);
        bool ready = g_analyzer.analyze(g_bands);

        TEST_ASSERT_TRUE_MESSAGE(ready, "Analyzer should be ready after 512 samples");

        // Find max band
        uint8_t maxBand = 0;
        float maxVal = g_bands[0];
        for (uint8_t i = 1; i < 8; ++i) {
            if (g_bands[i] > maxVal) {
                maxVal = g_bands[i];
                maxBand = i;
            }
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "Band %d detection failed (got %d)", band, maxBand);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(band, maxBand, msg);

        // Record for SNR calculation
        g_benchmark.recordSignal(g_bands, band);
    }

    printf("    All 8 bands correctly identified\n");
}

/**
 * @brief Test noise floor during silence
 */
void test_noise_floor_silence(void) {
    printf("\n  Testing noise floor during silence...\n");

    // Generate 10 frames of silence
    SignalConfig cfg;
    cfg.type = SignalType::SILENCE;

    for (int frame = 0; frame < 10; ++frame) {
        g_signalGen.generate(g_testBuffer, 512, cfg);

        g_analyzer.reset();
        g_analyzer.accumulate(g_testBuffer, 512);
        g_analyzer.analyze(g_bands);

        g_benchmark.recordNoiseFloor(g_bands);
    }

    // Verify all bands are below threshold
    float maxBand = *std::max_element(g_bands, g_bands + 8);
    TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(0.01f, maxBand,
        "Silence should produce near-zero magnitudes");

    printf("    Noise floor: max band = %.6f\n", maxBand);
}

/**
 * @brief Test SNR across all bands
 */
void test_snr_calculation(void) {
    printf("\n  Testing SNR calculation...\n");

    // First establish noise floor
    SignalConfig silenceCfg;
    silenceCfg.type = SignalType::SILENCE;

    for (int frame = 0; frame < 5; ++frame) {
        g_signalGen.generate(g_testBuffer, 512, silenceCfg);
        g_analyzer.reset();
        g_analyzer.accumulate(g_testBuffer, 512);
        g_analyzer.analyze(g_bands);
        g_benchmark.recordNoiseFloor(g_bands);
    }

    // Then measure signal at each frequency
    for (uint8_t band = 0; band < 8; ++band) {
        g_signalGen.generateGoertzelTarget(g_testBuffer, 512, band, 0.5f);
        g_analyzer.reset();
        g_analyzer.accumulate(g_testBuffer, 512);
        g_analyzer.analyze(g_bands);
        g_benchmark.recordSignal(g_bands, band);
    }

    // Finalize and check results
    BenchmarkResults results = g_benchmark.finalize("SNR Test", 1000.0f);

    printf("    Average SNR: %.1f dB\n", results.avgSnrDb);
    for (uint8_t i = 0; i < 8; ++i) {
        printf("    Band %d: %.1f dB\n", i, results.snrDb[i]);
    }

    // At least the average should be above minimum
    TEST_ASSERT_GREATER_THAN_FLOAT_MESSAGE(AudioPipelineBenchmark::SNR_MIN_DB,
        results.avgSnrDb, "Average SNR should be above minimum threshold");
}

// =============================================================================
// DYNAMIC RANGE TESTS
// =============================================================================

/**
 * @brief Test level sweep response
 */
void test_level_sweep_response(void) {
    printf("\n  Testing level sweep response...\n");

    g_benchmark.reset();

    SignalConfig cfg;
    cfg.type = SignalType::LEVEL_SWEEP;
    cfg.frequency = 1000.0f;
    cfg.startAmplitude = 0.01f;
    cfg.endAmplitude = 0.9f;
    cfg.sampleRate = 16000;

    // Generate 20 frames of level sweep (simulates 320ms)
    for (int frame = 0; frame < 20; ++frame) {
        g_signalGen.generate(g_testBuffer, 512, cfg);
        // Adjust sweep position for each frame
        cfg.startAmplitude = 0.01f + frame * 0.045f;
        cfg.endAmplitude = cfg.startAmplitude + 0.045f;

        g_analyzer.reset();
        g_analyzer.accumulate(g_testBuffer, 512);
        g_analyzer.analyze(g_bands);

        // Record the 1kHz band (band 4) as output
        g_benchmark.recordOutput(g_bands[4]);
        g_benchmark.recordSignal(g_bands, 4);
    }

    BenchmarkResults results = g_benchmark.finalize("Level Sweep", 320.0f);

    printf("    Dynamic range utilization: %.2f\n", results.dynamicRangeUtil);
    TEST_ASSERT_GREATER_THAN_FLOAT_MESSAGE(0.3f, results.dynamicRangeUtil,
        "Dynamic range should capture level variations");
}

// =============================================================================
// TRANSIENT RESPONSE TESTS
// =============================================================================

/**
 * @brief Test impulse detection
 *
 * Fixed: Use analyzeWindow() for precise control over impulse placement.
 * Place impulse at center (position 256) to ensure it's fully captured
 * within the 512-sample Goertzel window.
 */
void test_impulse_response(void) {
    printf("\n  Testing impulse response...\n");

    // Create 512-sample window with impulse at center
    int16_t tempBuffer[512];
    std::memset(tempBuffer, 0, 512 * sizeof(int16_t));

    // Place impulse burst at center (positions 254-258) for stronger signal
    const int16_t impulseValue = static_cast<int16_t>(0.9f * 32767.0f);
    for (int i = 254; i <= 258; ++i) {
        tempBuffer[i] = impulseValue;
    }

    g_analyzer.reset();
    bool ready = g_analyzer.analyzeWindow(tempBuffer, 512, g_bands);
    TEST_ASSERT_TRUE(ready);

    // Impulse should excite multiple bands (broadband signal)
    int activeBands = 0;
    printf("    Band magnitudes: ");
    for (uint8_t i = 0; i < 8; ++i) {
        printf("%.3f%s", g_bands[i], i < 7 ? ", " : "\n");
        if (g_bands[i] > 0.01f) {  // Threshold calibrated for 5-sample impulse burst
            activeBands++;
        }
    }

    printf("    Impulse excited %d bands\n", activeBands);
    TEST_ASSERT_GREATER_THAN_MESSAGE(3, activeBands,
        "Impulse should excite multiple frequency bands");
}

/**
 * @brief Test impulse train detection for beat tracking
 *
 * Fixed: Manually create impulse bursts every 4 frames (simulating 128ms intervals)
 * to ensure sufficient energy for detection. Use edge detection with
 * calibrated threshold.
 */
void test_impulse_train_detection(void) {
    printf("\n  Testing impulse train detection...\n");

    g_benchmark.reset();

    int transientCount = 0;
    bool wasHigh = false;  // State machine for edge detection

    // Process 2 seconds worth of audio (62 frames)
    // Place impulse burst every 4 frames (~128ms interval)
    for (int frame = 0; frame < 62; ++frame) {
        // Clear buffer
        std::memset(g_testBuffer, 0, 512 * sizeof(int16_t));

        // Every 4th frame, insert impulse burst at center
        if (frame % 4 == 0) {
            const int16_t impulseValue = static_cast<int16_t>(0.9f * 32767.0f);
            for (int i = 250; i <= 260; ++i) {
                g_testBuffer[i] = impulseValue;
            }
        }

        g_analyzer.reset();
        g_analyzer.accumulate(g_testBuffer, 512);
        g_analyzer.analyze(g_bands);

        // Calculate total energy
        float energy = 0.0f;
        for (uint8_t i = 0; i < 8; ++i) {
            energy += g_bands[i];
        }

        // Edge detection: transition from low to high energy
        bool isHigh = (energy > 0.05f);  // Threshold for 11-sample burst
        if (isHigh && !wasHigh) {
            transientCount++;
            if (transientCount <= 5) {  // Limit output
                printf("    Transient %d at frame %d (energy: %.3f)\n",
                       transientCount, frame, energy);
            }
        }
        wasHigh = isHigh;

        g_benchmark.recordSignal(g_bands, -1);
    }

    printf("    Detected %d transients (expected ~15 at 128ms intervals)\n", transientCount);
    TEST_ASSERT_GREATER_THAN_MESSAGE(10, transientCount,
        "Should detect multiple transients in impulse train");
}

// =============================================================================
// NOISE REJECTION TESTS
// =============================================================================

/**
 * @brief Test white noise rejection
 */
void test_white_noise_rejection(void) {
    printf("\n  Testing white noise rejection...\n");

    SignalConfig cfg;
    cfg.type = SignalType::WHITE_NOISE;
    cfg.amplitude = 0.3f;

    g_benchmark.reset();

    // Process 20 frames of white noise
    for (int frame = 0; frame < 20; ++frame) {
        g_signalGen.generate(g_testBuffer, 512, cfg);

        g_analyzer.reset();
        g_analyzer.accumulate(g_testBuffer, 512);
        g_analyzer.analyze(g_bands);

        // White noise should have relatively flat spectrum
        // Record as noise floor (potential false triggers)
        g_benchmark.recordNoiseFloor(g_bands);
    }

    BenchmarkResults results = g_benchmark.finalize("White Noise", 640.0f);

    printf("    False triggers during white noise: %u\n", results.falseTriggerCount);

    // White noise at 0.3 amplitude should trigger (it's not silence)
    // But should be relatively uniform across bands
    float bandVariance = 0.0f;
    float meanBand = 0.0f;
    for (uint8_t i = 0; i < 8; ++i) {
        meanBand += g_bands[i];
    }
    meanBand /= 8.0f;

    for (uint8_t i = 0; i < 8; ++i) {
        float diff = g_bands[i] - meanBand;
        bandVariance += diff * diff;
    }
    bandVariance /= 8.0f;

    printf("    Band variance: %.4f (flat spectrum expected)\n", bandVariance);
    // Flat spectrum means low variance
    TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(0.05f, bandVariance,
        "White noise should have relatively flat spectrum");
}

/**
 * @brief Test pink noise spectrum
 */
void test_pink_noise_spectrum(void) {
    printf("\n  Testing pink noise spectrum (-3dB/octave)...\n");

    SignalConfig cfg;
    cfg.type = SignalType::PINK_NOISE;
    cfg.amplitude = 0.5f;

    // Average across many frames for stable measurement
    float avgBands[8] = {0};

    for (int frame = 0; frame < 50; ++frame) {
        g_signalGen.generate(g_testBuffer, 512, cfg);

        g_analyzer.reset();
        g_analyzer.accumulate(g_testBuffer, 512);
        g_analyzer.analyze(g_bands);

        for (uint8_t i = 0; i < 8; ++i) {
            avgBands[i] += g_bands[i];
        }
    }

    for (uint8_t i = 0; i < 8; ++i) {
        avgBands[i] /= 50.0f;
        printf("    Band %d: %.4f\n", i, avgBands[i]);
    }

    // Pink noise: bass should be stronger than treble
    TEST_ASSERT_GREATER_THAN_FLOAT_MESSAGE(avgBands[6], avgBands[0],
        "Pink noise bass should exceed treble");
}

// =============================================================================
// PRESET COMPARISON TESTS
// =============================================================================

/**
 * @brief Compare audio presets with benchmark metrics
 */
void test_preset_comparison(void) {
    printf("\n  Comparing audio presets...\n");

    AudioPreset presets[] = {
        AudioPreset::LIGHTWAVE_V2,
        AudioPreset::SENSORY_BRIDGE,
        AudioPreset::AGGRESSIVE_AGC,
        AudioPreset::CONSERVATIVE_AGC
    };

    for (AudioPreset preset : presets) {
        AudioPipelineTuning tuning = getPreset(preset);
        const char* name = getPresetName(preset);

        printf("\n    Preset: %s\n", name);
        printf("      AGC Attack: %.3f, Release: %.3f (ratio %.1f:1)\n",
            tuning.agcAttack, tuning.agcRelease,
            tuning.agcAttack / tuning.agcRelease);
        printf("      Smoothing Fast: %.3f, Slow: %.3f\n",
            tuning.controlBusAlphaFast, tuning.controlBusAlphaSlow);
        printf("      Silence Hysteresis: %.0f ms\n", tuning.silenceHysteresisMs);
    }

    // Verify Sensory Bridge has expected 50:1 ratio
    AudioPipelineTuning sbTuning = getPreset(AudioPreset::SENSORY_BRIDGE);
    float ratio = sbTuning.agcAttack / sbTuning.agcRelease;

    TEST_ASSERT_GREATER_THAN_FLOAT_MESSAGE(40.0f, ratio,
        "Sensory Bridge preset should have ~50:1 AGC ratio");
}

// =============================================================================
// FULL BENCHMARK SUITE
// =============================================================================

/**
 * @brief Run complete benchmark for a single configuration
 */
void run_full_benchmark(const char* name) {
    printf("\n  Running full benchmark: %s\n", name);

    g_benchmark.reset();
    g_signalGen.seed(0xDEADBEEF);  // Consistent seed for reproducibility

    // Phase 1: Noise floor calibration (silence)
    printf("    Phase 1: Noise floor calibration...\n");
    SignalConfig silenceCfg;
    silenceCfg.type = SignalType::SILENCE;

    for (int frame = 0; frame < 30; ++frame) {
        g_signalGen.generate(g_testBuffer, 512, silenceCfg);
        g_analyzer.reset();
        g_analyzer.accumulate(g_testBuffer, 512);
        g_analyzer.analyze(g_bands);
        g_benchmark.recordNoiseFloor(g_bands);
    }

    // Phase 2: Spectral accuracy (per-band sine waves)
    printf("    Phase 2: Spectral accuracy...\n");
    for (uint8_t band = 0; band < 8; ++band) {
        for (int frame = 0; frame < 5; ++frame) {
            g_signalGen.generateGoertzelTarget(g_testBuffer, 512, band, 0.5f);
            g_analyzer.reset();
            g_analyzer.accumulate(g_testBuffer, 512);
            g_analyzer.analyze(g_bands);
            g_benchmark.recordSignal(g_bands, band);
            g_benchmark.recordOutput(g_bands[band]);
        }
    }

    // Phase 3: Dynamic range (level sweep)
    printf("    Phase 3: Dynamic range...\n");
    SignalConfig sweepCfg;
    sweepCfg.type = SignalType::LEVEL_SWEEP;
    sweepCfg.frequency = 500.0f;
    sweepCfg.startAmplitude = 0.01f;
    sweepCfg.endAmplitude = 0.9f;

    for (int frame = 0; frame < 20; ++frame) {
        g_signalGen.generate(g_testBuffer, 512, sweepCfg);
        g_analyzer.reset();
        g_analyzer.accumulate(g_testBuffer, 512);
        g_analyzer.analyze(g_bands);
        g_benchmark.recordOutput(g_bands[3]);  // 500 Hz = band 3
    }

    // Finalize results
    BenchmarkResults results = g_benchmark.finalize(name, 2000.0f);

    // Print summary
    char buffer[1024];
    g_benchmark.formatResults(buffer, sizeof(buffer));
    printf("\n%s\n", buffer);
}

void test_full_benchmark_lightwave(void) {
    run_full_benchmark("LightwaveOS v2");
}

// =============================================================================
// MAIN
// =============================================================================

int main(void) {
    printf("\n");
    printf("================================================================\n");
    printf("  LightwaveOS v2 - Audio Pipeline Benchmark Suite\n");
    printf("  Based on Sensory Bridge Comparative Analysis\n");
    printf("================================================================\n");

    UNITY_BEGIN();

    // Spectral accuracy tests
    printf("\n--- SPECTRAL ACCURACY ---\n");
    RUN_TEST(test_spectral_accuracy_per_band);
    RUN_TEST(test_noise_floor_silence);
    RUN_TEST(test_snr_calculation);

    // Dynamic range tests
    printf("\n--- DYNAMIC RANGE ---\n");
    RUN_TEST(test_level_sweep_response);

    // Transient response tests
    printf("\n--- TRANSIENT RESPONSE ---\n");
    RUN_TEST(test_impulse_response);
    RUN_TEST(test_impulse_train_detection);

    // Noise rejection tests
    printf("\n--- NOISE REJECTION ---\n");
    RUN_TEST(test_white_noise_rejection);
    RUN_TEST(test_pink_noise_spectrum);

    // Preset comparison
    printf("\n--- PRESET COMPARISON ---\n");
    RUN_TEST(test_preset_comparison);

    // Full benchmark
    printf("\n--- FULL BENCHMARK ---\n");
    RUN_TEST(test_full_benchmark_lightwave);

    int result = UNITY_END();

    printf("\n================================================================\n");
    if (result == 0) {
        printf("  All benchmark tests passed!\n");
    } else {
        printf("  Some tests failed. Review results above.\n");
    }
    printf("================================================================\n\n");

    return result;
}
