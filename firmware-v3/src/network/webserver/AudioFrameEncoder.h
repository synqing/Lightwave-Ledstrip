// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file AudioFrameEncoder.h
 * @brief Audio frame encoder for WebSocket binary streaming
 *
 * Encodes ControlBusFrame data into binary format for real-time streaming.
 * Frame format: [MAGIC][HOP_SEQ][TIMESTAMP][RMS/FLUX][BANDS][CHROMA][WAVEFORM]
 *
 * All data is little-endian for efficient JavaScript DataView parsing.
 */

#pragma once

#include "AudioStreamConfig.h"
#include "../../config/features.h"

#if FEATURE_AUDIO_SYNC

#include "../../audio/contracts/ControlBus.h"
#include "../../audio/contracts/MusicalGrid.h"
#include <stdint.h>
#include <stddef.h>
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {

/**
 * @brief Encodes ControlBusFrame into binary format for WebSocket streaming
 *
 * Thread-safe: caller must ensure frame is not modified during encoding.
 * Buffer must be at least AudioStreamConfig::FRAME_SIZE bytes.
 */
class AudioFrameEncoder {
public:
    /**
     * @brief Encode ControlBusFrame and MusicalGridSnapshot into binary format
     * @param frame Source audio frame
     * @param grid Source musical grid snapshot
     * @param timestampMs Current timestamp in milliseconds
     * @param outputBuffer Output buffer (must be at least FRAME_SIZE bytes)
     * @param bufferSize Size of output buffer
     * @return Number of bytes written, or 0 on error
     */
    static size_t encode(const audio::ControlBusFrame& frame,
                         const audio::MusicalGridSnapshot& grid,
                         uint32_t timestampMs,
                         uint8_t* outputBuffer,
                         size_t bufferSize) {
        using namespace AudioStreamConfig;

        if (!outputBuffer || bufferSize < FRAME_SIZE) return 0;

        // Clear buffer
        memset(outputBuffer, 0, FRAME_SIZE);

        // Header
        memcpy(outputBuffer + OFF_MAGIC, &MAGIC, sizeof(uint32_t));
        memcpy(outputBuffer + OFF_HOP_SEQ, &frame.hop_seq, sizeof(uint32_t));
        memcpy(outputBuffer + OFF_TIMESTAMP, &timestampMs, sizeof(uint32_t));

        // Core metrics
        memcpy(outputBuffer + OFF_RMS, &frame.rms, sizeof(float));
        memcpy(outputBuffer + OFF_FLUX, &frame.flux, sizeof(float));
        memcpy(outputBuffer + OFF_FAST_RMS, &frame.fast_rms, sizeof(float));
        memcpy(outputBuffer + OFF_FAST_FLUX, &frame.fast_flux, sizeof(float));

        // Band data
        memcpy(outputBuffer + OFF_BANDS, frame.bands, NUM_BANDS * sizeof(float));
        memcpy(outputBuffer + OFF_HEAVY_BANDS, frame.heavy_bands, NUM_BANDS * sizeof(float));

        // Chroma data
        memcpy(outputBuffer + OFF_CHROMA, frame.chroma, NUM_CHROMA * sizeof(float));
        memcpy(outputBuffer + OFF_HEAVY_CHROMA, frame.heavy_chroma, NUM_CHROMA * sizeof(float));

        // Waveform (int16_t[128])
        memcpy(outputBuffer + OFF_WAVEFORM, frame.waveform, WAVEFORM_SIZE * sizeof(int16_t));

        // MusicalGrid data (16 bytes)
        memcpy(outputBuffer + OFF_BPM_SMOOTHED, &grid.bpm_smoothed, sizeof(float));
        memcpy(outputBuffer + OFF_TEMPO_CONFIDENCE, &grid.tempo_confidence, sizeof(float));
        memcpy(outputBuffer + OFF_BEAT_PHASE01, &grid.beat_phase01, sizeof(float));

        // Convert bool to uint8_t for consistent wire format
        uint8_t beat_tick = grid.beat_tick ? 1 : 0;
        uint8_t downbeat_tick = grid.downbeat_tick ? 1 : 0;
        outputBuffer[OFF_BEAT_TICK] = beat_tick;
        outputBuffer[OFF_DOWNBEAT_TICK] = downbeat_tick;
        // Reserved bytes already zeroed by memset

        return FRAME_SIZE;
    }

    /**
     * @brief Validate frame format
     * @param buffer Frame buffer
     * @param bufferSize Size of buffer
     * @return true if valid, false otherwise
     */
    static bool validate(const uint8_t* buffer, size_t bufferSize) {
        if (!buffer || bufferSize < AudioStreamConfig::FRAME_SIZE) return false;

        // Check magic
        uint32_t magic = 0;
        memcpy(&magic, buffer + AudioStreamConfig::OFF_MAGIC, sizeof(uint32_t));
        return magic == AudioStreamConfig::MAGIC;
    }

    /**
     * @brief Get frame size for current format
     */
    static constexpr size_t getFrameSize() {
        return AudioStreamConfig::FRAME_SIZE;
    }
};

} // namespace webserver
} // namespace network
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
