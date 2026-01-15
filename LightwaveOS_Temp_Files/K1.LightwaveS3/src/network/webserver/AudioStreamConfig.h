/**
 * @file AudioStreamConfig.h
 * @brief Configuration constants for audio stream broadcasting
 *
 * Defines frame format, timing, and limits for real-time audio metric streaming.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace lightwaveos {
namespace network {
namespace webserver {

namespace AudioStreamConfig {
    // Stream version and identification
    constexpr uint8_t STREAM_VERSION = 1;            // Frame format version
    constexpr uint32_t MAGIC = 0x00445541;           // "AUD\0" little-endian

    // Audio data dimensions (must match ControlBusFrame)
    constexpr uint8_t NUM_BANDS = 8;                 // Frequency bands
    constexpr uint8_t NUM_CHROMA = 12;               // Pitch classes (C-B)
    constexpr uint8_t WAVEFORM_SIZE = 128;           // Time-domain samples

    // Frame structure offsets (448 bytes total)
    // Header: 12 bytes
    constexpr size_t OFF_MAGIC = 0;                  // uint32_t magic (4 bytes)
    constexpr size_t OFF_HOP_SEQ = 4;                // uint32_t hop sequence (4 bytes)
    constexpr size_t OFF_TIMESTAMP = 8;              // uint32_t timestamp_ms (4 bytes)

    // Core metrics: 16 bytes
    constexpr size_t OFF_RMS = 12;                   // float rms
    constexpr size_t OFF_FLUX = 16;                  // float flux
    constexpr size_t OFF_FAST_RMS = 20;              // float fast_rms
    constexpr size_t OFF_FAST_FLUX = 24;             // float fast_flux

    // Band data: 64 bytes
    constexpr size_t OFF_BANDS = 28;                 // float[8] bands
    constexpr size_t OFF_HEAVY_BANDS = 60;           // float[8] heavy_bands

    // Chroma data: 96 bytes
    constexpr size_t OFF_CHROMA = 92;                // float[12] chroma
    constexpr size_t OFF_HEAVY_CHROMA = 140;         // float[12] heavy_chroma

    // Reserved + Waveform: 260 bytes
    constexpr size_t OFF_RESERVED = 188;             // 4 bytes padding
    constexpr size_t OFF_WAVEFORM = 192;             // int16_t[128] waveform (256 bytes)

    // MusicalGrid data: 16 bytes (starts at offset 448)
    constexpr size_t OFF_BPM_SMOOTHED = 448;         // float bpm_smoothed (4 bytes)
    constexpr size_t OFF_TEMPO_CONFIDENCE = 452;     // float tempo_confidence (4 bytes)
    constexpr size_t OFF_BEAT_PHASE01 = 456;         // float beat_phase01 (4 bytes)
    constexpr size_t OFF_BEAT_TICK = 460;            // uint8_t beat_tick (1 byte)
    constexpr size_t OFF_DOWNBEAT_TICK = 461;        // uint8_t downbeat_tick (1 byte)
    constexpr size_t OFF_MUSICAL_RESERVED = 462;     // uint8_t[2] reserved padding (2 bytes)

    // Total frame size
    constexpr size_t FRAME_SIZE = 464;

    // Streaming configuration
    constexpr uint8_t MAX_CLIENTS = 4;               // Max simultaneous subscribers
    constexpr uint8_t TARGET_FPS = 30;               // Broadcast rate (matches audio hop rate)
    constexpr uint32_t FRAME_INTERVAL_MS = 1000 / TARGET_FPS;  // ~33ms between frames
}

} // namespace webserver
} // namespace network
} // namespace lightwaveos
