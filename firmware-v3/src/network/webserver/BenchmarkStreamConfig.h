/**
 * @file BenchmarkStreamConfig.h
 * @brief Configuration constants for benchmark metrics streaming
 *
 * Defines frame format, timing, and limits for real-time audio pipeline
 * performance metrics streaming. Follows AudioStreamConfig pattern.
 *
 * Binary frame format optimized for:
 * - Low bandwidth (32 bytes compact, 64 bytes extended)
 * - Fast parsing (fixed offsets, no JSON overhead)
 * - Cross-platform compatibility (little-endian, packed)
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace lightwaveos {
namespace network {
namespace webserver {

namespace BenchmarkStreamConfig {
    // Stream version and identification
    constexpr uint8_t STREAM_VERSION = 1;            // Frame format version
    constexpr uint32_t MAGIC = 0x004D4241;           // "ABM\0" (Audio Benchmark) little-endian

    // Frame types
    enum class FrameType : uint8_t {
        STATS_COMPACT = 0x01,    // 32-byte aggregated stats
        STATS_EXTENDED = 0x02,   // 64-byte with histogram
        SAMPLE_SINGLE = 0x03,    // Single 32-byte timing sample
        SAMPLE_BATCH = 0x04      // Batch of N samples
    };

    //=========================================================================
    // Compact Frame (32 bytes) - sent at 10 Hz
    //=========================================================================
    // Header: 8 bytes
    constexpr size_t COMPACT_OFF_MAGIC = 0;           // uint32_t magic
    constexpr size_t COMPACT_OFF_TIMESTAMP = 4;       // uint32_t timestamp_ms

    // Core metrics: 20 bytes
    constexpr size_t COMPACT_OFF_AVG_TOTAL_US = 8;    // float avgTotalUs
    constexpr size_t COMPACT_OFF_AVG_GOERTZEL_US = 12;// float avgGoertzelUs
    constexpr size_t COMPACT_OFF_CPU_LOAD = 16;       // float cpuLoadPercent
    constexpr size_t COMPACT_OFF_PEAK_TOTAL_US = 20;  // uint16_t peakTotalUs
    constexpr size_t COMPACT_OFF_PEAK_GOERTZEL_US = 22;// uint16_t peakGoertzelUs
    constexpr size_t COMPACT_OFF_HOP_COUNT = 24;      // uint32_t hopCount

    // Status: 4 bytes
    constexpr size_t COMPACT_OFF_GOERTZEL_COUNT = 28; // uint16_t goertzelCount (low 16 bits)
    constexpr size_t COMPACT_OFF_FLAGS = 30;          // uint8_t flags (streaming state)
    constexpr size_t COMPACT_OFF_RESERVED = 31;       // uint8_t reserved

    constexpr size_t COMPACT_FRAME_SIZE = 32;

    //=========================================================================
    // Extended Frame (64 bytes) - on demand or every 1 second
    //=========================================================================
    // Includes all compact fields + histogram

    // Header + metrics: 32 bytes (same as compact)
    constexpr size_t EXTENDED_OFF_MAGIC = COMPACT_OFF_MAGIC;
    constexpr size_t EXTENDED_OFF_TIMESTAMP = COMPACT_OFF_TIMESTAMP;
    constexpr size_t EXTENDED_OFF_AVG_TOTAL_US = COMPACT_OFF_AVG_TOTAL_US;
    constexpr size_t EXTENDED_OFF_AVG_GOERTZEL_US = COMPACT_OFF_AVG_GOERTZEL_US;
    constexpr size_t EXTENDED_OFF_CPU_LOAD = COMPACT_OFF_CPU_LOAD;
    constexpr size_t EXTENDED_OFF_PEAK_TOTAL_US = COMPACT_OFF_PEAK_TOTAL_US;
    constexpr size_t EXTENDED_OFF_PEAK_GOERTZEL_US = COMPACT_OFF_PEAK_GOERTZEL_US;
    constexpr size_t EXTENDED_OFF_HOP_COUNT = COMPACT_OFF_HOP_COUNT;

    // Additional metrics: 16 bytes
    constexpr size_t EXTENDED_OFF_AVG_DC_AGC_US = 32; // float avgDcAgcUs
    constexpr size_t EXTENDED_OFF_AVG_CHROMA_US = 36; // float avgChromaUs
    constexpr size_t EXTENDED_OFF_GOERTZEL_COUNT_FULL = 40; // uint32_t goertzelCount (full)
    constexpr size_t EXTENDED_OFF_RESERVED2 = 44;     // uint32_t reserved

    // Histogram: 16 bytes (8 x uint16_t bins)
    constexpr size_t EXTENDED_OFF_HISTOGRAM = 48;     // uint16_t[8] histogramBins

    constexpr size_t EXTENDED_FRAME_SIZE = 64;

    //=========================================================================
    // Streaming configuration
    //=========================================================================
    constexpr uint8_t MAX_CLIENTS = 4;                // Max simultaneous subscribers
    constexpr uint8_t TARGET_FPS = 10;                // Broadcast rate (10 Hz)
    constexpr uint32_t FRAME_INTERVAL_MS = 1000 / TARGET_FPS;  // 100ms between frames

    // Flag bits
    constexpr uint8_t FLAG_STREAMING_ACTIVE = 0x01;
    constexpr uint8_t FLAG_BENCHMARK_ENABLED = 0x02;
    constexpr uint8_t FLAG_PEAKS_RESET = 0x04;
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos
