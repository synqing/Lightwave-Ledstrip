// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AudioPipelineBenchmark.h
 * @brief Metrics collection and analysis for audio pipeline benchmarking
 *
 * Implements quantitative metrics from the validation framework:
 * - SNR per frequency band
 * - False trigger rate during silence
 * - Dynamic range utilization
 * - Latency measurements
 * - Statistical analysis
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace lightwaveos::audio::test {

/**
 * @brief Benchmark results for a single test configuration
 */
struct BenchmarkResults {
    // Configuration identification
    char presetName[32] = {0};

    // Primary metrics (from validation framework)
    float snrDb[8] = {0};           ///< SNR per band in dB
    float avgSnrDb = 0.0f;          ///< Average SNR across all bands
    uint32_t falseTriggerCount = 0; ///< LED activations during silence
    float dynamicRangeUtil = 0.0f;  ///< Spread of mapped values [0, 1]
    float avgLatencyMs = 0.0f;      ///< Audio-to-LED latency
    float cpuLoadPercent = 0.0f;    ///< Processing time as % of budget

    // Secondary metrics
    float bandEnergyCorrelation = 0.0f;  ///< Cross-correlation between adjacent bands
    float agcSettlingTimeMs = 0.0f;      ///< Time to reach 90% target gain

    // Pass/fail thresholds (from validation framework)
    bool passSnr = false;           ///< All bands >= 20 dB, avg >= 35 dB
    bool passFalseTrigger = false;  ///< <= 1 per minute
    bool passDynamicRange = false;  ///< >= 0.5
    bool passLatency = false;       ///< <= 20 ms
    bool passCpuLoad = false;       ///< <= 30%
    bool passOverall = false;       ///< All criteria pass
};

/**
 * @brief Running statistics accumulator
 */
class RunningStats {
public:
    void reset() {
        m_count = 0;
        m_sum = 0.0;
        m_sumSq = 0.0;
        m_min = 1e30;
        m_max = -1e30;
    }

    void add(double value) {
        m_count++;
        m_sum += value;
        m_sumSq += value * value;
        if (value < m_min) m_min = value;
        if (value > m_max) m_max = value;
    }

    size_t count() const { return m_count; }
    double mean() const { return m_count > 0 ? m_sum / static_cast<double>(m_count) : 0.0; }
    double variance() const {
        if (m_count < 2) return 0.0;
        double m = mean();
        return (m_sumSq / static_cast<double>(m_count)) - (m * m);
    }
    double stdDev() const { return std::sqrt(variance()); }
    double min() const { return m_count > 0 ? m_min : 0.0; }
    double max() const { return m_count > 0 ? m_max : 0.0; }
    double range() const { return max() - min(); }

private:
    size_t m_count = 0;
    double m_sum = 0.0;
    double m_sumSq = 0.0;
    double m_min = 1e30;
    double m_max = -1e30;
};

/**
 * @brief Audio pipeline benchmark metrics collector
 *
 * Collects and analyzes metrics during benchmark runs to enable
 * A/B comparison between audio pipeline configurations.
 */
class AudioPipelineBenchmark {
public:
    static constexpr uint8_t NUM_BANDS = 8;
    static constexpr float SNR_TARGET_DB = 35.0f;
    static constexpr float SNR_MIN_DB = 20.0f;
    static constexpr float LATENCY_MAX_MS = 20.0f;
    static constexpr float CPU_LOAD_MAX_PERCENT = 30.0f;
    static constexpr float DYNAMIC_RANGE_MIN = 0.5f;
    static constexpr uint32_t FALSE_TRIGGER_MAX_PER_MIN = 1;

    /**
     * @brief Reset all metrics for a new benchmark run
     */
    void reset() {
        m_results = BenchmarkResults();
        for (uint8_t i = 0; i < NUM_BANDS; ++i) {
            m_bandStats[i].reset();
        }
        m_outputStats.reset();
        m_latencyStats.reset();
        m_cpuLoadStats.reset();

        m_silenceFrames = 0;
        m_falseTriggerCount = 0;
        m_noiseFloor[0] = m_noiseFloor[1] = m_noiseFloor[2] = m_noiseFloor[3] = 0.0f;
        m_noiseFloor[4] = m_noiseFloor[5] = m_noiseFloor[6] = m_noiseFloor[7] = 0.0f;
        m_signalPower[0] = m_signalPower[1] = m_signalPower[2] = m_signalPower[3] = 0.0f;
        m_signalPower[4] = m_signalPower[5] = m_signalPower[6] = m_signalPower[7] = 0.0f;
    }

    /**
     * @brief Record band magnitudes during silence (for noise floor)
     *
     * @param bands Array of 8 band magnitudes [0, 1]
     */
    void recordNoiseFloor(const float* bands) {
        for (uint8_t i = 0; i < NUM_BANDS; ++i) {
            // Track max as noise floor (worst case)
            if (bands[i] > m_noiseFloor[i]) {
                m_noiseFloor[i] = bands[i];
            }
        }
        m_silenceFrames++;

        // Check for false trigger (any band > threshold during silence)
        float maxBand = *std::max_element(bands, bands + NUM_BANDS);
        if (maxBand > 0.1f) {  // 10% threshold for false trigger
            m_falseTriggerCount++;
        }
    }

    /**
     * @brief Record band magnitudes during signal (for SNR calculation)
     *
     * @param bands Array of 8 band magnitudes [0, 1]
     * @param targetBand The band that should have signal (-1 for all)
     */
    void recordSignal(const float* bands, int8_t targetBand = -1) {
        for (uint8_t i = 0; i < NUM_BANDS; ++i) {
            m_bandStats[i].add(bands[i]);

            // Track signal power for target band(s)
            if (targetBand < 0 || i == targetBand) {
                if (bands[i] > m_signalPower[i]) {
                    m_signalPower[i] = bands[i];
                }
            }
        }
    }

    /**
     * @brief Record output value for dynamic range analysis
     *
     * @param outputValue The mapped output value (e.g., LED brightness)
     */
    void recordOutput(float outputValue) {
        m_outputStats.add(outputValue);
    }

    /**
     * @brief Record a latency measurement
     *
     * @param latencyMs Measured audio-to-LED latency in milliseconds
     */
    void recordLatency(float latencyMs) {
        m_latencyStats.add(latencyMs);
    }

    /**
     * @brief Record CPU processing time
     *
     * @param processingTimeUs Processing time in microseconds
     * @param budgetUs Time budget in microseconds (e.g., 16ms for 62.5 Hz)
     */
    void recordCpuTime(uint32_t processingTimeUs, uint32_t budgetUs) {
        float percent = 100.0f * static_cast<float>(processingTimeUs) /
                       static_cast<float>(budgetUs);
        m_cpuLoadStats.add(percent);
    }

    /**
     * @brief Compute final benchmark results
     *
     * @param presetName Name of the configuration being tested
     * @param testDurationMs Duration of the test in milliseconds
     * @return Completed BenchmarkResults structure
     */
    BenchmarkResults finalize(const char* presetName, float testDurationMs) {
        // Copy preset name
        std::strncpy(m_results.presetName, presetName, sizeof(m_results.presetName) - 1);

        // Calculate SNR for each band
        float snrSum = 0.0f;
        bool allBandsPass = true;

        for (uint8_t i = 0; i < NUM_BANDS; ++i) {
            float noise = m_noiseFloor[i] > 0.0f ? m_noiseFloor[i] : 1e-10f;
            float signal = m_signalPower[i];

            if (signal > noise) {
                m_results.snrDb[i] = 20.0f * std::log10(signal / noise);
            } else {
                m_results.snrDb[i] = 0.0f;
            }

            snrSum += m_results.snrDb[i];
            if (m_results.snrDb[i] < SNR_MIN_DB) {
                allBandsPass = false;
            }
        }

        m_results.avgSnrDb = snrSum / NUM_BANDS;
        m_results.passSnr = allBandsPass && (m_results.avgSnrDb >= SNR_TARGET_DB);

        // False trigger rate
        m_results.falseTriggerCount = m_falseTriggerCount;
        float testMinutes = testDurationMs / 60000.0f;
        float triggerPerMin = testMinutes > 0.0f ?
            static_cast<float>(m_falseTriggerCount) / testMinutes : 0.0f;
        m_results.passFalseTrigger = (triggerPerMin <= FALSE_TRIGGER_MAX_PER_MIN);

        // Dynamic range utilization
        m_results.dynamicRangeUtil = static_cast<float>(m_outputStats.range());
        m_results.passDynamicRange = (m_results.dynamicRangeUtil >= DYNAMIC_RANGE_MIN);

        // Latency
        m_results.avgLatencyMs = static_cast<float>(m_latencyStats.mean());
        m_results.passLatency = (m_results.avgLatencyMs <= LATENCY_MAX_MS);

        // CPU load
        m_results.cpuLoadPercent = static_cast<float>(m_cpuLoadStats.mean());
        m_results.passCpuLoad = (m_results.cpuLoadPercent <= CPU_LOAD_MAX_PERCENT);

        // Overall pass
        m_results.passOverall = m_results.passSnr &&
                                m_results.passFalseTrigger &&
                                m_results.passDynamicRange &&
                                m_results.passLatency &&
                                m_results.passCpuLoad;

        // Band correlation (adjacent band similarity)
        float corrSum = 0.0f;
        for (uint8_t i = 0; i < NUM_BANDS - 1; ++i) {
            float a = static_cast<float>(m_bandStats[i].mean());
            float b = static_cast<float>(m_bandStats[i + 1].mean());
            if (a > 0.0f && b > 0.0f) {
                float minVal = std::min(a, b);
                float maxVal = std::max(a, b);
                corrSum += minVal / maxVal;
            }
        }
        m_results.bandEnergyCorrelation = corrSum / (NUM_BANDS - 1);

        return m_results;
    }

    /**
     * @brief Print benchmark results summary to a buffer
     *
     * @param buffer Output buffer
     * @param bufferSize Size of output buffer
     * @return Number of characters written
     */
    int formatResults(char* buffer, size_t bufferSize) const {
        int written = 0;
        written += snprintf(buffer + written, bufferSize - written,
            "=== Benchmark Results: %s ===\n", m_results.presetName);

        written += snprintf(buffer + written, bufferSize - written,
            "SNR (dB): %.1f avg [", m_results.avgSnrDb);
        for (uint8_t i = 0; i < NUM_BANDS; ++i) {
            written += snprintf(buffer + written, bufferSize - written,
                "%.1f%s", m_results.snrDb[i], i < 7 ? ", " : "");
        }
        written += snprintf(buffer + written, bufferSize - written,
            "] %s\n", m_results.passSnr ? "PASS" : "FAIL");

        written += snprintf(buffer + written, bufferSize - written,
            "False Triggers: %u %s\n",
            m_results.falseTriggerCount,
            m_results.passFalseTrigger ? "PASS" : "FAIL");

        written += snprintf(buffer + written, bufferSize - written,
            "Dynamic Range: %.2f %s\n",
            m_results.dynamicRangeUtil,
            m_results.passDynamicRange ? "PASS" : "FAIL");

        written += snprintf(buffer + written, bufferSize - written,
            "Latency: %.1f ms %s\n",
            m_results.avgLatencyMs,
            m_results.passLatency ? "PASS" : "FAIL");

        written += snprintf(buffer + written, bufferSize - written,
            "CPU Load: %.1f%% %s\n",
            m_results.cpuLoadPercent,
            m_results.passCpuLoad ? "PASS" : "FAIL");

        written += snprintf(buffer + written, bufferSize - written,
            "\nOVERALL: %s\n", m_results.passOverall ? "PASS" : "FAIL");

        return written;
    }

    /**
     * @brief Get the current results (read-only)
     */
    const BenchmarkResults& results() const { return m_results; }

private:
    BenchmarkResults m_results;

    // Per-band statistics
    RunningStats m_bandStats[NUM_BANDS];

    // Output value statistics
    RunningStats m_outputStats;

    // Latency statistics
    RunningStats m_latencyStats;

    // CPU load statistics
    RunningStats m_cpuLoadStats;

    // Noise floor tracking
    float m_noiseFloor[NUM_BANDS];
    float m_signalPower[NUM_BANDS];

    // False trigger detection
    size_t m_silenceFrames = 0;
    uint32_t m_falseTriggerCount = 0;
};

} // namespace lightwaveos::audio::test
