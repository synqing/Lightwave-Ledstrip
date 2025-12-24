#define FEATURE_AUDIO_SYNC 1

#include <unity.h>
#include "../../src/audio/GoertzelAnalyzer.h"
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace lightwaveos::audio;

void generateSineWave(int16_t* buffer, size_t numSamples, float frequency,
                      uint32_t sampleRate, int16_t amplitude = 16000) {
    for (size_t i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float sample = amplitude * std::sin(2.0f * M_PI * frequency * t + M_PI / 4.0f);
        buffer[i] = static_cast<int16_t>(sample);
    }
}

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

void setUp(void) {
}

void tearDown(void) {
}

void test_target_frequencies(void) {
    GoertzelAnalyzer analyzer;
    int16_t testSamples[512];
    float bands[8];

    const float TARGET_FREQS[8] = {60, 120, 250, 500, 1000, 2000, 4000, 7800};

    for (uint8_t targetBand = 0; targetBand < 8; ++targetBand) {
        generateSineWave(testSamples, 512, TARGET_FREQS[targetBand], 16000);
        analyzer.reset();
        analyzer.accumulate(testSamples, 512);
        bool ready = analyzer.analyze(bands);
        TEST_ASSERT_TRUE_MESSAGE(ready, "Analyzer should be ready after 512 samples");

        uint8_t detectedBand = findMaxIndex(bands, 8);
        float detectedMagnitude = bands[detectedBand];

        char msg[100];
        snprintf(msg, sizeof(msg), "Expected band %d (%.0f Hz), got band %d", targetBand, TARGET_FREQS[targetBand], detectedBand);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(targetBand, detectedBand, msg);

        snprintf(msg, sizeof(msg), "Magnitude too low: %.3f (Band %d)", detectedMagnitude, targetBand);
        TEST_ASSERT_GREATER_THAN_FLOAT_MESSAGE(0.3f, detectedMagnitude, msg);
    }
}

void test_silence(void) {
    GoertzelAnalyzer analyzer;
    int16_t testSamples[512];
    float bands[8];

    std::memset(testSamples, 0, sizeof(testSamples));
    analyzer.reset();
    analyzer.accumulate(testSamples, 512);

    bool ready = analyzer.analyze(bands);
    TEST_ASSERT_TRUE(ready);

    float maxMagnitude = bands[findMaxIndex(bands, 8)];
    TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(0.01f, maxMagnitude, "Silence should produce near-zero magnitudes");
}

void test_multi_hop_accumulation(void) {
    GoertzelAnalyzer analyzer;
    int16_t testSamples[512];
    float bands[8];

    generateSineWave(testSamples, 512, 500.0f, 16000);
    analyzer.reset();

    analyzer.accumulate(testSamples, 256);
    bool ready1 = analyzer.analyze(bands);
    TEST_ASSERT_FALSE_MESSAGE(ready1, "Analyzer should NOT be ready after 256 samples");

    analyzer.accumulate(testSamples + 256, 256);
    bool ready2 = analyzer.analyze(bands);
    TEST_ASSERT_TRUE_MESSAGE(ready2, "Analyzer SHOULD be ready after 512 samples");

    uint8_t detectedBand = findMaxIndex(bands, 8);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, detectedBand, "Should detect 500Hz band after accumulation");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_target_frequencies);
    RUN_TEST(test_silence);
    RUN_TEST(test_multi_hop_accumulation);
    return UNITY_END();
}
