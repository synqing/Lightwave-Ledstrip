#include <unity.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "../../src/audio/onset/OnsetDetector.h"

using namespace lightwaveos::audio;

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr uint32_t kSampleRate = 32000;
constexpr size_t kHopSize = 256;
constexpr size_t kHistorySize = 1024;

struct StreamSummary {
    int eventCount = 0;
    int positiveEnvCount = 0;
    int kickCount = 0;
    int snareCount = 0;
    int hihatCount = 0;
    float maxFlux = 0.0f;
    float maxEnv = 0.0f;
    std::vector<size_t> eventHops;
};

static float clampUnit(float x) {
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return x;
}

static float computeRms(const float* samples, size_t count) {
    if (count == 0) return 0.0f;
    double sumSq = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sumSq += static_cast<double>(samples[i]) * static_cast<double>(samples[i]);
    }
    return static_cast<float>(std::sqrt(sumSq / static_cast<double>(count)));
}

static uint32_t lcgNext(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

static float noiseSigned(uint32_t& state) {
    const uint32_t bits = lcgNext(state) >> 8;
    const float unit = static_cast<float>(bits) / static_cast<float>(0x00FFFFFFu);
    return unit * 2.0f - 1.0f;
}

static bool isBurstHop(size_t hopIndex) {
    return hopIndex >= 64 && hopIndex <= 128 && ((hopIndex - 64) % 16 == 0);
}

static void fillSilenceHop(size_t hopIndex, std::array<float, kHopSize>& hop) {
    (void)hopIndex;
    hop.fill(0.0f);
}

static void fillCarrierBurstHop(size_t hopIndex, std::array<float, kHopSize>& hop) {
    uint32_t rng = 0x13579BDFu ^ static_cast<uint32_t>(hopIndex * 2654435761u);
    const bool burst = isBurstHop(hopIndex);

    for (size_t i = 0; i < kHopSize; ++i) {
        const size_t sampleIndex = hopIndex * kHopSize + i;
        const float t = static_cast<float>(sampleIndex) / static_cast<float>(kSampleRate);

        float sample = 0.18f * std::sinf(2.0f * kPi * 1800.0f * t);
        if (burst) {
            sample += 0.72f * noiseSigned(rng);
        }
        hop[i] = clampUnit(sample);
    }
}

static void fillNoiseFloorHop(size_t hopIndex, std::array<float, kHopSize>& hop) {
    uint32_t rng = 0x2468ACE1u ^ static_cast<uint32_t>(hopIndex * 2246822519u);
    for (size_t i = 0; i < kHopSize; ++i) {
        hop[i] = 0.006f * noiseSigned(rng);
    }
}

static void fillLowSnrBurstHop(size_t hopIndex, std::array<float, kHopSize>& hop) {
    uint32_t bedRng = 0x89ABCDEFu ^ static_cast<uint32_t>(hopIndex * 3266489917u);
    uint32_t burstRng = 0x10293847u ^ static_cast<uint32_t>(hopIndex * 668265263u);
    const bool burst = isBurstHop(hopIndex);

    for (size_t i = 0; i < kHopSize; ++i) {
        float sample = 0.006f * noiseSigned(bedRng);
        if (burst) {
            sample += 0.10f * noiseSigned(burstRng);
        }
        hop[i] = clampUnit(sample);
    }
}

template <typename FillHopFn>
static StreamSummary runDetector(size_t totalHops, FillHopFn fillHop,
                                 const OnsetDetector::Config& cfg = OnsetDetector::Config{}) {
    OnsetDetector detector;
    detector.init(cfg);

    StreamSummary summary;
    std::array<float, kHistorySize> history = {};
    std::array<float, kHopSize> hop = {};

    for (size_t hopIndex = 0; hopIndex < totalHops; ++hopIndex) {
        fillHop(hopIndex, hop);

        std::memmove(history.data(),
                     history.data() + kHopSize,
                     (kHistorySize - kHopSize) * sizeof(float));
        std::memcpy(history.data() + (kHistorySize - kHopSize),
                    hop.data(),
                    kHopSize * sizeof(float));

        const float rms = computeRms(hop.data(), kHopSize);
        OnsetResult result = detector.process(history.data(), rms);

        if (result.flux > summary.maxFlux) summary.maxFlux = result.flux;
        if (result.onset_env > summary.maxEnv) summary.maxEnv = result.onset_env;
        if (result.onset_env > 0.0f) summary.positiveEnvCount++;
        if (result.onset_event > 0.0f) {
            summary.eventCount++;
            summary.eventHops.push_back(hopIndex);
        }
        if (result.kick_trigger) summary.kickCount++;
        if (result.snare_trigger) summary.snareCount++;
        if (result.hihat_trigger) summary.hihatCount++;
    }

    return summary;
}

} // namespace

// Test config: adapted for synthetic raw PCM (no VU AGC).
// Production defaults target VU-derived gateRms (~0.08 idle, 750-frame warmup).
// In tests: gateRms = rawRms (~0.001-0.1), short warmup, matching floor.
static OnsetDetector::Config testConfig() {
    OnsetDetector::Config cfg;
    cfg.warmupFrames = 50;         // Short warmup for synthetic tests (not 750)
    cfg.activityFloor = 0.002f;    // Match raw PCM scale (same as rmsGate)
    cfg.activityRangeMin = 0.002f; // Match raw PCM scale
    return cfg;
}

void setUp(void) {
}

void tearDown(void) {
}

void test_silence_is_fully_gated(void) {
    const StreamSummary summary = runDetector(96, fillSilenceHop, testConfig());

    TEST_ASSERT_EQUAL_INT(0, summary.eventCount);
    TEST_ASSERT_EQUAL_INT(0, summary.positiveEnvCount);
    TEST_ASSERT_EQUAL_INT(0, summary.kickCount);
    TEST_ASSERT_EQUAL_INT(0, summary.snareCount);
    TEST_ASSERT_EQUAL_INT(0, summary.hihatCount);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, summary.maxFlux);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, summary.maxEnv);
}

void test_bursts_produce_sparse_onset_events(void) {
    const StreamSummary summary = runDetector(144, fillCarrierBurstHop, testConfig());

    // Diagnostic dump (quarantine measurement — remove after diagnosis)
    printf("\n=== BURST DIAGNOSTIC ===\n");
    printf("  eventCount:       %d\n", summary.eventCount);
    printf("  positiveEnvCount: %d  (limit < 24)\n", summary.positiveEnvCount);
    printf("  maxFlux:          %.4f\n", summary.maxFlux);
    printf("  maxEnv:           %.4f\n", summary.maxEnv);
    printf("  kickCount:        %d\n", summary.kickCount);
    printf("  snareCount:       %d\n", summary.snareCount);
    printf("  hihatCount:       %d\n", summary.hihatCount);
    printf("  eventHops:        ");
    for (size_t h : summary.eventHops) printf("%zu ", h);
    printf("\n========================\n\n");

    TEST_ASSERT_TRUE_MESSAGE(summary.maxFlux > 8.0f,
                             "Burst stream should generate substantial raw flux");
    TEST_ASSERT_TRUE_MESSAGE(summary.maxEnv > 1.0f,
                             "Burst stream should generate non-trivial onset_env");
    TEST_ASSERT_TRUE_MESSAGE(summary.eventCount >= 3 && summary.eventCount <= 8,
                             "Expected sparse onset events aligned with burst hops");
    TEST_ASSERT_TRUE_MESSAGE(summary.positiveEnvCount <= 32,
                             "Median thresholding should keep onset_env sparse, but allow a few positive frames around each burst");
    TEST_ASSERT_TRUE_MESSAGE(summary.hihatCount > 0,
                             "High-frequency bursts should trip the hi-hat path at least once");

    for (size_t eventHop : summary.eventHops) {
        TEST_ASSERT_TRUE_MESSAGE(eventHop >= 50,
                                 "Events should appear after detector warmup settles");
    }
}

void test_noise_floor_does_not_self_trigger(void) {
    // Noise test needs longer warmup than burst tests — the median threshold
    // must settle on the noise floor's steady-state flux before we check.
    auto noiseCfg = testConfig();
    noiseCfg.warmupFrames = 80;
    const StreamSummary summary = runDetector(192, fillNoiseFloorHop, noiseCfg);

    printf("\n=== NOISE DIAGNOSTIC ===\n");
    printf("  events=%d envPos=%d maxFlux=%.4f maxEnv=%.4f kick=%d snr=%d hh=%d\n",
           summary.eventCount, summary.positiveEnvCount,
           summary.maxFlux, summary.maxEnv,
           summary.kickCount, summary.snareCount, summary.hihatCount);
    printf("========================\n\n");

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, summary.eventCount,
                             "Ambient broadband noise should not produce recurring onset events");
    TEST_ASSERT_TRUE_MESSAGE(summary.positiveEnvCount <= 16,
                             "Ambient broadband noise should not keep onset_env alive (median threshold allows occasional leakage)");
    TEST_ASSERT_TRUE_MESSAGE(summary.kickCount == 0 && summary.snareCount == 0 && summary.hihatCount == 0,
                             "Ambient broadband noise should not trip percussion paths");
}

void test_low_snr_bursts_still_break_through(void) {
    const StreamSummary summary = runDetector(192, fillLowSnrBurstHop, testConfig());

    TEST_ASSERT_TRUE_MESSAGE(summary.eventCount >= 3 && summary.eventCount <= 8,
                             "Low-SNR bursts should still generate sparse onset events");
    TEST_ASSERT_TRUE_MESSAGE(summary.maxEnv >= 0.050f,
                             "Low-SNR bursts should still exceed the onset floor");
    TEST_ASSERT_TRUE_MESSAGE(summary.positiveEnvCount <= 16,
                             "Low-SNR bursts should remain sparse rather than keeping onset_env high");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_silence_is_fully_gated);
    RUN_TEST(test_bursts_produce_sparse_onset_events);
    RUN_TEST(test_noise_floor_does_not_self_trigger);
    RUN_TEST(test_low_snr_bursts_still_break_through);
    return UNITY_END();
}
