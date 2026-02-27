// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#define FEATURE_AUDIO_SYNC 1

#include <unity.h>
#include "../../src/audio/GoertzelAnalyzer.h"
#include "../../src/audio/tempo/TempoTracker.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>

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

float computeRms01(const int16_t* samples, size_t count) {
    if (count == 0) return 0.0f;
    double sumSq = 0.0;
    for (size_t i = 0; i < count; ++i) {
        double x = static_cast<double>(samples[i]) / 32768.0;
        sumSq += x * x;
    }
    return static_cast<float>(std::sqrt(sumSq / static_cast<double>(count)));
}

void generateImpulseTrainBlock(int16_t* out, size_t count, uint32_t sampleRate,
                               float bpm, float amplitude01, uint64_t sampleBase) {
    std::memset(out, 0, count * sizeof(int16_t));
    double intervalSamplesF = (static_cast<double>(sampleRate) * 60.0) / static_cast<double>(bpm);
    uint64_t intervalSamples = static_cast<uint64_t>(intervalSamplesF + 0.5);
    if (intervalSamples == 0) intervalSamples = 1;
    int16_t impulse = static_cast<int16_t>(std::max(0.0f, std::min(1.0f, amplitude01)) * 32767.0f);

    for (size_t i = 0; i < count; ++i) {
        uint64_t s = sampleBase + static_cast<uint64_t>(i);
        if ((s % intervalSamples) == 0) out[i] = impulse;
    }
}

void generateDualImpulseTrainBlock(int16_t* out, size_t count, uint32_t sampleRate,
                                   float bpmPrimary, float ampPrimary01,
                                   float bpmSecondary, float ampSecondary01,
                                   uint64_t sampleBase) {
    std::memset(out, 0, count * sizeof(int16_t));
    double intPrimaryF = (static_cast<double>(sampleRate) * 60.0) / static_cast<double>(bpmPrimary);
    double intSecondaryF = (static_cast<double>(sampleRate) * 60.0) / static_cast<double>(bpmSecondary);
    uint64_t intPrimary = static_cast<uint64_t>(intPrimaryF + 0.5);
    uint64_t intSecondary = static_cast<uint64_t>(intSecondaryF + 0.5);
    if (intPrimary == 0) intPrimary = 1;
    if (intSecondary == 0) intSecondary = 1;
    int16_t impulsePrimary = static_cast<int16_t>(std::max(0.0f, std::min(1.0f, ampPrimary01)) * 32767.0f);
    int16_t impulseSecondary = static_cast<int16_t>(std::max(0.0f, std::min(1.0f, ampSecondary01)) * 32767.0f);

    for (size_t i = 0; i < count; ++i) {
        uint64_t s = sampleBase + static_cast<uint64_t>(i);
        int32_t v = 0;
        if ((s % intPrimary) == 0) v += impulsePrimary;
        if ((s % intSecondary) == 0) v += impulseSecondary;
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        out[i] = static_cast<int16_t>(v);
    }
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

void runTempoCase(float bpm, float toleranceBpm, uint32_t seconds, bool withSubdivision) {
    GoertzelAnalyzer analyzer;
    TempoTracker tempo;
    tempo.init();

    float bands[8] = {0};
    int16_t hop[256];

    const uint32_t sampleRate = 16000;
    const float hopSec = 256.0f / static_cast<float>(sampleRate);
    const uint32_t totalHops = static_cast<uint32_t>((static_cast<uint64_t>(seconds) * sampleRate) / 256u);

    uint64_t sampleBase = 0;
    for (uint32_t hopIdx = 0; hopIdx < totalHops; ++hopIdx) {
        if (withSubdivision) {
            generateDualImpulseTrainBlock(hop, 256, sampleRate, bpm, 0.85f, bpm * 2.0f, 0.25f, sampleBase);
        } else {
            generateImpulseTrainBlock(hop, 256, sampleRate, bpm, 0.85f, sampleBase);
        }

        analyzer.accumulate(hop, 256);
        bool bandsReady = analyzer.analyze(bands);

        float rms01 = computeRms01(hop, 256);
        tempo.updateNovelty(bandsReady ? bands : nullptr, 8, rms01, bandsReady);
        tempo.updateTempo(hopSec);
        tempo.advancePhase(hopSec);

        sampleBase += 256;
    }

    auto out = tempo.getOutput();
    char msg[160];
    snprintf(msg, sizeof(msg), "Expected %.1f BPM, got %.2f (conf=%.3f)", bpm, out.bpm, out.confidence);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(toleranceBpm, bpm, out.bpm, msg);
    TEST_ASSERT_GREATER_THAN_FLOAT_MESSAGE(0.05f, out.confidence, "Tempo confidence too low");
}

void test_tempo_impulse_train_60bpm(void) {
    runTempoCase(60.0f, 1.5f, 16, false);
}

void test_tempo_impulse_train_90bpm(void) {
    runTempoCase(90.0f, 1.5f, 16, false);
}

void test_tempo_impulse_train_120bpm(void) {
    runTempoCase(120.0f, 1.5f, 16, false);
}

void test_tempo_impulse_train_138bpm(void) {
    runTempoCase(138.0f, 1.5f, 16, false);
}

void test_tempo_138bpm_with_subdivision(void) {
    runTempoCase(138.0f, 1.5f, 16, true);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_target_frequencies);
    RUN_TEST(test_silence);
    RUN_TEST(test_multi_hop_accumulation);
    RUN_TEST(test_tempo_impulse_train_60bpm);
    RUN_TEST(test_tempo_impulse_train_90bpm);
    RUN_TEST(test_tempo_impulse_train_120bpm);
    RUN_TEST(test_tempo_impulse_train_138bpm);
    RUN_TEST(test_tempo_138bpm_with_subdivision);
    return UNITY_END();
}
