// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file BenchmarkFrameEncoder.h
 * @brief Binary frame encoder for benchmark metrics
 *
 * Encodes AudioBenchmarkStats into compact binary frames for
 * efficient WebSocket transmission. Little-endian format.
 */

#pragma once

#include "../../config/features.h"

#if FEATURE_AUDIO_BENCHMARK

#include "BenchmarkStreamConfig.h"
#include "../../audio/AudioBenchmarkMetrics.h"
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {

/**
 * @brief Encodes benchmark stats into binary frames
 */
class BenchmarkFrameEncoder {
public:
    /**
     * @brief Encode stats into compact frame (32 bytes)
     * @param stats Source stats from AudioActor
     * @param timestamp_ms Current timestamp in milliseconds
     * @param flags Status flags (streaming active, etc.)
     * @param[out] buffer Output buffer (must be >= COMPACT_FRAME_SIZE)
     */
    static void encodeCompact(
        const audio::AudioBenchmarkStats& stats,
        uint32_t timestamp_ms,
        uint8_t flags,
        uint8_t* buffer)
    {
        using namespace BenchmarkStreamConfig;

        // Clear buffer
        std::memset(buffer, 0, COMPACT_FRAME_SIZE);

        // Header
        writeU32(buffer, COMPACT_OFF_MAGIC, MAGIC);
        writeU32(buffer, COMPACT_OFF_TIMESTAMP, timestamp_ms);

        // Core metrics
        writeF32(buffer, COMPACT_OFF_AVG_TOTAL_US, stats.avgTotalUs);
        writeF32(buffer, COMPACT_OFF_AVG_GOERTZEL_US, stats.avgGoertzelUs);
        writeF32(buffer, COMPACT_OFF_CPU_LOAD, stats.cpuLoadPercent);
        writeU16(buffer, COMPACT_OFF_PEAK_TOTAL_US, stats.peakTotalUs);
        writeU16(buffer, COMPACT_OFF_PEAK_GOERTZEL_US, stats.peakGoertzelUs);
        writeU32(buffer, COMPACT_OFF_HOP_COUNT, stats.hopCount);

        // Status
        writeU16(buffer, COMPACT_OFF_GOERTZEL_COUNT, static_cast<uint16_t>(stats.goertzelCount & 0xFFFF));
        buffer[COMPACT_OFF_FLAGS] = flags;
    }

    /**
     * @brief Encode stats into extended frame (64 bytes)
     * @param stats Source stats from AudioActor
     * @param timestamp_ms Current timestamp in milliseconds
     * @param flags Status flags
     * @param[out] buffer Output buffer (must be >= EXTENDED_FRAME_SIZE)
     */
    static void encodeExtended(
        const audio::AudioBenchmarkStats& stats,
        uint32_t timestamp_ms,
        uint8_t flags,
        uint8_t* buffer)
    {
        using namespace BenchmarkStreamConfig;

        // Start with compact frame
        encodeCompact(stats, timestamp_ms, flags, buffer);

        // Additional metrics
        writeF32(buffer, EXTENDED_OFF_AVG_DC_AGC_US, stats.avgDcAgcUs);
        writeF32(buffer, EXTENDED_OFF_AVG_CHROMA_US, stats.avgChromaUs);
        writeU32(buffer, EXTENDED_OFF_GOERTZEL_COUNT_FULL, stats.goertzelCount);

        // Histogram
        for (size_t i = 0; i < 8; ++i) {
            writeU16(buffer, EXTENDED_OFF_HISTOGRAM + i * 2, stats.histogramBins[i]);
        }
    }

    /**
     * @brief Encode a single timing sample (32 bytes)
     * @param sample Source sample
     * @param[out] buffer Output buffer (must be >= 32 bytes)
     */
    static void encodeSample(
        const audio::AudioBenchmarkSample& sample,
        uint8_t* buffer)
    {
        // Direct memcpy - sample is already packed and 32 bytes
        std::memcpy(buffer, &sample, sizeof(audio::AudioBenchmarkSample));
    }

private:
    static void writeU16(uint8_t* buf, size_t offset, uint16_t val) {
        buf[offset] = val & 0xFF;
        buf[offset + 1] = (val >> 8) & 0xFF;
    }

    static void writeU32(uint8_t* buf, size_t offset, uint32_t val) {
        buf[offset] = val & 0xFF;
        buf[offset + 1] = (val >> 8) & 0xFF;
        buf[offset + 2] = (val >> 16) & 0xFF;
        buf[offset + 3] = (val >> 24) & 0xFF;
    }

    static void writeF32(uint8_t* buf, size_t offset, float val) {
        uint32_t bits;
        std::memcpy(&bits, &val, sizeof(float));
        writeU32(buf, offset, bits);
    }
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_BENCHMARK
