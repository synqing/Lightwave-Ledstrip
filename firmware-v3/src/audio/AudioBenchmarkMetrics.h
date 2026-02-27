// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AudioBenchmarkMetrics.h
 * @brief Per-hop timing metrics for audio pipeline performance analysis
 *
 * Provides low-overhead instrumentation for measuring processing times
 * within AudioActor::processHop(). Designed for ESP32-S3 with:
 * - 32-byte packed samples for cache efficiency
 * - Lock-free ring buffer for cross-core access
 * - <0.02% CPU overhead target
 *
 * Enable with FEATURE_AUDIO_BENCHMARK=1 in platformio.ini
 */

#pragma once

#include <cstdint>
#include <cstring>

namespace lightwaveos::audio {

/**
 * @brief Single timing sample from one processHop() invocation
 *
 * Packed to 32 bytes (power of 2) for efficient ring buffer indexing.
 * All timing fields use uint16_t (max 65ms, sufficient for 16ms hop budget).
 */
struct AudioBenchmarkSample {
    uint32_t timestamp_us;          ///< esp_timer_get_time() low 32 bits

    // Phase timings in microseconds
    uint16_t dcAgcLoopUs;           ///< DC removal + AGC gain application
    uint16_t rmsComputeUs;          ///< RMS + flux calculation
    uint16_t goertzelUs;            ///< 8-band Goertzel analysis (0 if skipped)
    uint16_t chromaUs;              ///< 12-pitch chromagram (0 if skipped)
    uint16_t controlBusUs;          ///< ControlBus smoothing + frame build
    uint16_t publishUs;             ///< SnapshotBuffer publish
    uint16_t totalProcessUs;        ///< Full processHop() duration

    // Auxiliary metrics
    uint16_t captureReadUs;         ///< I2S DMA read time (0 if not measured)
    uint8_t  goertzelTriggered;     ///< 1 if Goertzel ran this hop
    uint8_t  chromaTriggered;       ///< 1 if chroma ran this hop

    // Padding to 32 bytes (4 + 14 + 4 + 10 = 32)
    uint8_t  _padding[10];

    void clear() {
        std::memset(this, 0, sizeof(*this));
    }
} __attribute__((packed));

static_assert(sizeof(AudioBenchmarkSample) == 32, "Sample must be 32 bytes for ring buffer efficiency");

/// Ring buffer configuration
constexpr size_t BENCHMARK_RING_SIZE = 64;  ///< ~1.3 seconds at 50 Hz hop rate @ 12.8kHz
constexpr size_t BENCHMARK_RING_MASK = BENCHMARK_RING_SIZE - 1;

/// Frame budget for CPU load calculation (20ms = 20000us @ 12.8kHz)
constexpr uint32_t HOP_BUDGET_US = 20000;

/// Histogram bin edges for latency distribution (microseconds)
constexpr uint16_t HISTOGRAM_BIN_EDGES[8] = {
    100, 200, 400, 800, 1600, 3200, 6400, UINT16_MAX
};

/**
 * @brief Aggregated statistics computed from ring buffer samples
 *
 * Updated periodically (every ~50 hops = 1 second @ 12.8kHz) to minimize overhead.
 * Published via separate SnapshotBuffer for WebSocket streaming.
 */
struct AudioBenchmarkStats {
    // Counters (since start or last reset)
    uint32_t hopCount;              ///< Total hops processed
    uint32_t goertzelCount;         ///< Hops where Goertzel triggered

    // Rolling averages (exponential moving average, alpha=0.1)
    float avgTotalUs;               ///< Average processHop() time
    float avgDcAgcUs;               ///< Average DC/AGC loop time
    float avgGoertzelUs;            ///< Average Goertzel time (when triggered)
    float avgChromaUs;              ///< Average chroma time (when triggered)

    // Peaks (reset on read via resetPeaks())
    uint16_t peakTotalUs;           ///< Max processHop() time
    uint16_t peakGoertzelUs;        ///< Max Goertzel time

    // Derived metrics
    float cpuLoadPercent;           ///< avgTotalUs / HOP_BUDGET_US * 100

    // Latency distribution histogram (8 bins)
    // Bins: <100, <200, <400, <800, <1600, <3200, <6400, >=6400 us
    uint16_t histogramBins[8];

    void reset() {
        hopCount = 0;
        goertzelCount = 0;
        avgTotalUs = 0.0f;
        avgDcAgcUs = 0.0f;
        avgGoertzelUs = 0.0f;
        avgChromaUs = 0.0f;
        peakTotalUs = 0;
        peakGoertzelUs = 0;
        cpuLoadPercent = 0.0f;
        std::memset(histogramBins, 0, sizeof(histogramBins));
    }

    void resetPeaks() {
        peakTotalUs = 0;
        peakGoertzelUs = 0;
    }

    /**
     * @brief Update stats with a new sample using EMA
     * @param sample The timing sample to incorporate
     * @param alpha EMA smoothing factor (default 0.1 for ~10 sample window)
     */
    void updateFromSample(const AudioBenchmarkSample& sample, float alpha = 0.1f) {
        hopCount++;

        // Update averages with EMA
        if (hopCount == 1) {
            // First sample: initialize directly
            avgTotalUs = sample.totalProcessUs;
            avgDcAgcUs = sample.dcAgcLoopUs;
        } else {
            avgTotalUs = alpha * sample.totalProcessUs + (1.0f - alpha) * avgTotalUs;
            avgDcAgcUs = alpha * sample.dcAgcLoopUs + (1.0f - alpha) * avgDcAgcUs;
        }

        // Track Goertzel stats only when triggered
        if (sample.goertzelTriggered) {
            goertzelCount++;
            if (goertzelCount == 1) {
                avgGoertzelUs = sample.goertzelUs;
            } else {
                avgGoertzelUs = alpha * sample.goertzelUs + (1.0f - alpha) * avgGoertzelUs;
            }
            if (sample.goertzelUs > peakGoertzelUs) {
                peakGoertzelUs = sample.goertzelUs;
            }
        }

        // Track chroma stats only when triggered
        if (sample.chromaTriggered) {
            if (avgChromaUs == 0.0f) {
                avgChromaUs = sample.chromaUs;
            } else {
                avgChromaUs = alpha * sample.chromaUs + (1.0f - alpha) * avgChromaUs;
            }
        }

        // Update peaks
        if (sample.totalProcessUs > peakTotalUs) {
            peakTotalUs = sample.totalProcessUs;
        }

        // Update CPU load
        cpuLoadPercent = (avgTotalUs / static_cast<float>(HOP_BUDGET_US)) * 100.0f;

        // Update histogram
        uint8_t bin = 7;  // Default to highest bin
        for (uint8_t i = 0; i < 7; ++i) {
            if (sample.totalProcessUs < HISTOGRAM_BIN_EDGES[i]) {
                bin = i;
                break;
            }
        }
        histogramBins[bin]++;
    }
};

} // namespace lightwaveos::audio
