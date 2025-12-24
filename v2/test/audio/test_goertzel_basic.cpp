/**
 * @file test_goertzel_basic.cpp
 * @brief Basic verification test for GoertzelAnalyzer
 *
 * This test generates synthetic sine waves at target frequencies
 * and verifies that the Goertzel analyzer correctly detects them.
 */

#define FEATURE_AUDIO_SYNC 1  // Enable audio subsystem for test

#include "../../src/audio/GoertzelAnalyzer.h"
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace lightwaveos::audio;

/**
 * @brief Generate a pure sine wave at a specific frequency
 * @param buffer Output buffer for int16_t samples
 * @param numSamples Number of samples to generate
 * @param frequency Frequency in Hz
 * @param sampleRate Sample rate in Hz
 * @param amplitude Peak amplitude (0-32767)
 */
void generateSineWave(int16_t* buffer, size_t numSamples, float frequency,
                      uint32_t sampleRate, int16_t amplitude = 16000) {
    for (size_t i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float sample = amplitude * std::sin(2.0f * M_PI * frequency * t);
        buffer[i] = static_cast<int16_t>(sample);
    }
}

/**
 * @brief Find the index of the maximum value in an array
 */
uint8_t findMaxIndex(const float* array, size_t size) {
    uint8_t maxIdx = 0;
    float maxVal = array[0];
    for (size_t i = 1; i < size; ++i) {
        if (array[i] > maxVal) {
            maxVal = array[i];
            maxIdx = i;
        }
    }
    return maxIdx;
}

int main() {
    printf("=== GoertzelAnalyzer Basic Verification Test ===\n\n");

    GoertzelAnalyzer analyzer;
    int16_t testSamples[512];
    float bands[8];

    // Test frequencies matching the analyzer's target bands
    const float TARGET_FREQS[8] = {60, 120, 250, 500, 1000, 2000, 4000, 8000};
    const char* BAND_NAMES[8] = {
        "Sub-bass", "Bass", "Low-mid", "Mid",
        "High-mid", "Presence", "Brilliance", "Air"
    };

    printf("Target frequencies:\n");
    for (uint8_t i = 0; i < 8; ++i) {
        printf("  Band %d: %.0f Hz (%s)\n", i, TARGET_FREQS[i], BAND_NAMES[i]);
    }
    printf("\n");

    // Test each target frequency
    bool allTestsPassed = true;

    for (uint8_t targetBand = 0; targetBand < 8; ++targetBand) {
        printf("Test %d: Generating %.0f Hz sine wave...\n",
               targetBand + 1, TARGET_FREQS[targetBand]);

        // Generate sine wave at target frequency
        generateSineWave(testSamples, 512, TARGET_FREQS[targetBand], 16000);

        // Reset analyzer for fresh test
        analyzer.reset();

        // Accumulate samples
        analyzer.accumulate(testSamples, 512);

        // Analyze
        bool ready = analyzer.analyze(bands);

        if (!ready) {
            printf("  ERROR: Analyzer did not return results!\n");
            allTestsPassed = false;
            continue;
        }

        // Find which band has the strongest response
        uint8_t detectedBand = findMaxIndex(bands, 8);
        float detectedMagnitude = bands[detectedBand];

        printf("  Detected: Band %d (%.0f Hz) with magnitude %.3f\n",
               detectedBand, TARGET_FREQS[detectedBand], detectedMagnitude);

        // Print all band values
        printf("  All bands: [");
        for (uint8_t i = 0; i < 8; ++i) {
            printf("%.2f%s", bands[i], (i < 7) ? ", " : "");
        }
        printf("]\n");

        // Verify correct band was detected
        if (detectedBand == targetBand) {
            printf("  PASS: Correct frequency detected\n");
        } else {
            printf("  FAIL: Expected band %d, got band %d\n",
                   targetBand, detectedBand);
            allTestsPassed = false;
        }

        // Verify magnitude is reasonable (should be > 0.5 for pure sine wave)
        if (detectedMagnitude > 0.3f) {
            printf("  PASS: Magnitude is reasonable (%.3f > 0.3)\n",
                   detectedMagnitude);
        } else {
            printf("  WARN: Magnitude is low (%.3f < 0.3)\n",
                   detectedMagnitude);
        }

        printf("\n");
    }

    // Test silence (all zeros)
    printf("Test 9: Silence (all zeros)...\n");
    std::memset(testSamples, 0, sizeof(testSamples));
    analyzer.reset();
    analyzer.accumulate(testSamples, 512);

    if (analyzer.analyze(bands)) {
        float maxMagnitude = bands[findMaxIndex(bands, 8)];
        printf("  Max magnitude: %.4f\n", maxMagnitude);

        if (maxMagnitude < 0.01f) {
            printf("  PASS: Silence produces near-zero magnitudes\n");
        } else {
            printf("  FAIL: Silence should produce near-zero magnitudes\n");
            allTestsPassed = false;
        }
    }
    printf("\n");

    // Test accumulation across multiple calls
    printf("Test 10: Multi-hop accumulation (2x 256 samples)...\n");
    generateSineWave(testSamples, 512, 500.0f, 16000);  // 500 Hz (band 3)
    analyzer.reset();

    // First hop - should not be ready
    analyzer.accumulate(testSamples, 256);
    bool ready1 = analyzer.analyze(bands);
    printf("  After 256 samples: ready=%s\n", ready1 ? "true" : "false");

    // Second hop - should be ready
    analyzer.accumulate(testSamples + 256, 256);
    bool ready2 = analyzer.analyze(bands);
    printf("  After 512 samples: ready=%s\n", ready2 ? "true" : "false");

    if (!ready1 && ready2) {
        uint8_t detectedBand = findMaxIndex(bands, 8);
        if (detectedBand == 3) {  // 500 Hz is band 3
            printf("  PASS: Correct accumulation behavior\n");
        } else {
            printf("  FAIL: Expected band 3, got band %d\n", detectedBand);
            allTestsPassed = false;
        }
    } else {
        printf("  FAIL: Incorrect accumulation behavior\n");
        allTestsPassed = false;
    }
    printf("\n");

    // Final summary
    printf("=== Test Summary ===\n");
    if (allTestsPassed) {
        printf("ALL TESTS PASSED ✓\n");
        return 0;
    } else {
        printf("SOME TESTS FAILED ✗\n");
        return 1;
    }
}
