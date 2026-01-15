/**
 * @file ValidationFrameEncoder.h
 * @brief Binary frame encoder for effect validation samples
 *
 * Drains EffectValidationSample structs from EffectValidationRing and encodes
 * them into compact binary frames for WebSocket transmission.
 *
 * Binary frame format:
 * - 4-byte header: magic (bytes 0-2: 0x54, 0x56, 0x56 = "TVV"), sample count (byte 3)
 * - N x 128-byte EffectValidationSample structs (max 16 per frame)
 * - Max frame size: 2052 bytes (4 header + 16 * 128 samples)
 *
 * Full magic when combined: 0x4C565654 = "LVVT" (Lightwave Validation)
 * Little-endian format, no dynamic allocation.
 */

#pragma once

#include "../config/features.h"

#if FEATURE_EFFECT_VALIDATION

#include "EffectValidationMetrics.h"
#include <stdint.h>
#include <stddef.h>
#include <cstring>

#if defined(ESP32)
#include <ESPAsyncWebServer.h>
#endif

namespace lightwaveos {
namespace validation {

//=============================================================================
// Configuration Constants
//=============================================================================

namespace ValidationStreamConfig {
    // Frame identification
    // Magic bytes: 0x54 'T', 0x56 'V', 0x56 'V', then sample count
    // When read as uint32_t (ignoring sample count byte): lower 24 bits = 0x565654
    constexpr uint32_t MAGIC = 0x4C565654;              // "LVVT" little-endian (Lightwave Validation)
    constexpr uint8_t MAGIC_BYTE_0 = 0x54;              // 'T'
    constexpr uint8_t MAGIC_BYTE_1 = 0x56;              // 'V'
    constexpr uint8_t MAGIC_BYTE_2 = 0x56;              // 'V'

    // Frame limits
    constexpr size_t MAX_SAMPLES_PER_FRAME = 16;        // Max samples per frame
    constexpr size_t SAMPLE_SIZE = 128;                 // Size of EffectValidationSample
    constexpr size_t HEADER_SIZE = 4;                   // 4-byte header
    constexpr size_t MAX_FRAME_SIZE = HEADER_SIZE + (MAX_SAMPLES_PER_FRAME * SAMPLE_SIZE);  // 2052 bytes

    // Timing
    constexpr uint8_t DEFAULT_DRAIN_RATE_HZ = 10;       // Default drain rate (10 Hz)
    constexpr uint32_t DEFAULT_DRAIN_INTERVAL_MS = 1000 / DEFAULT_DRAIN_RATE_HZ;  // 100ms

    // Header offsets
    constexpr size_t HEADER_OFF_MAGIC_0 = 0;            // Magic byte 0 ('T')
    constexpr size_t HEADER_OFF_MAGIC_1 = 1;            // Magic byte 1 ('V')
    constexpr size_t HEADER_OFF_MAGIC_2 = 2;            // Magic byte 2 ('V')
    constexpr size_t HEADER_OFF_SAMPLE_COUNT = 3;       // Sample count (0-16)
}

//=============================================================================
// ValidationFrameEncoder - Encodes samples for WebSocket transmission
//=============================================================================

/**
 * @brief Drains validation samples and encodes binary frames
 *
 * Consumes samples from EffectValidationRing at configurable rate and
 * encodes them into binary frames suitable for WebSocket transmission.
 * No dynamic allocation - uses fixed internal buffer.
 *
 * Usage:
 *   1. Create encoder instance
 *   2. Call begin() with pointer to EffectValidationRing
 *   3. Call tick() from main loop with current timestamp
 *   4. When tick() returns true, call getFrame()/getFrameSize() to get data
 *   5. Send via WebSocket, then call clearFrame()
 *
 * Example:
 *   ValidationFrameEncoder encoder;
 *   EffectValidationRing<128> ring;
 *   encoder.begin(&ring);
 *
 *   // In main loop:
 *   if (encoder.tick(millis())) {
 *       ws->binaryAll(encoder.getFrame(), encoder.getFrameSize());
 *       encoder.clearFrame();
 *   }
 */
class ValidationFrameEncoder {
public:
    ValidationFrameEncoder()
        : m_ring(nullptr)
        , m_drainIntervalMs(ValidationStreamConfig::DEFAULT_DRAIN_INTERVAL_MS)
        , m_lastDrainTime(0)
        , m_frameReady(false)
        , m_frameSize(0)
        , m_sampleCount(0)
    {
        std::memset(m_frameBuffer, 0, sizeof(m_frameBuffer));
    }

    /**
     * @brief Initialize encoder with a ring buffer
     * @param ring Pointer to EffectValidationRing (any capacity)
     */
    template<size_t N>
    void begin(EffectValidationRing<N>* ring) {
        m_ring = reinterpret_cast<void*>(ring);
        m_ringDrain = [](void* r, EffectValidationSample* out, size_t max) -> size_t {
            return reinterpret_cast<EffectValidationRing<N>*>(r)->drain(out, max);
        };
        m_ringEmpty = [](void* r) -> bool {
            return reinterpret_cast<EffectValidationRing<N>*>(r)->empty();
        };
        m_ringAvailable = [](void* r) -> size_t {
            return reinterpret_cast<EffectValidationRing<N>*>(r)->available();
        };
        m_frameReady = false;
        m_frameSize = 0;
        m_lastDrainTime = 0;
    }

    /**
     * @brief Set drain rate
     * @param rateHz Drain rate in Hz (1-60)
     */
    void setDrainRate(uint8_t rateHz) {
        if (rateHz < 1) rateHz = 1;
        if (rateHz > 60) rateHz = 60;
        m_drainIntervalMs = 1000 / rateHz;
    }

    /**
     * @brief Get current drain rate
     * @return Drain rate in Hz
     */
    uint8_t getDrainRate() const {
        return static_cast<uint8_t>(1000 / m_drainIntervalMs);
    }

    /**
     * @brief Tick the encoder - call from main loop
     *
     * Checks if drain interval has elapsed, drains samples from ring,
     * and encodes a new frame if samples are available.
     *
     * @param currentTimeMs Current time in milliseconds
     * @return true if a new frame is ready for transmission
     */
    bool tick(uint32_t currentTimeMs) {
        if (!m_ring) {
            return false;
        }

        // Check if drain interval has elapsed
        if (currentTimeMs - m_lastDrainTime < m_drainIntervalMs) {
            return false;
        }
        m_lastDrainTime = currentTimeMs;

        // Check if there are samples to drain
        if (m_ringEmpty(m_ring)) {
            m_frameReady = false;
            return false;
        }

        // Drain samples and encode frame
        encodeFrame();
        return m_frameReady;
    }

    /**
     * @brief Tick using millis() - convenience for ESP32/Arduino
     * @return true if a new frame is ready
     */
    bool tick() {
#if defined(ESP32) || defined(ARDUINO)
        // Use global Arduino millis() - must use :: to escape namespace
        return tick(static_cast<uint32_t>(::millis()));
#else
        return false;
#endif
    }

    /**
     * @brief Get pointer to encoded frame buffer
     * @return Pointer to frame data (valid until next tick)
     */
    const uint8_t* getFrame() const {
        return m_frameBuffer;
    }

    /**
     * @brief Get size of encoded frame
     * @return Frame size in bytes (0 if no frame ready)
     */
    size_t getFrameSize() const {
        return m_frameReady ? m_frameSize : 0;
    }

    /**
     * @brief Check if a frame is ready for transmission
     */
    bool isFrameReady() const {
        return m_frameReady;
    }

    /**
     * @brief Get number of samples in current frame
     */
    uint16_t getSampleCount() const {
        return m_sampleCount;
    }

    /**
     * @brief Get number of samples available in ring buffer
     */
    size_t getAvailableSamples() const {
        if (!m_ring) return 0;
        return m_ringAvailable(m_ring);
    }

    /**
     * @brief Clear frame ready flag after transmission
     */
    void clearFrame() {
        m_frameReady = false;
        m_frameSize = 0;
        m_sampleCount = 0;
    }

#if defined(ESP32)
    /**
     * @brief Send validation frame to a WebSocket client
     * @param ws WebSocket server instance
     * @param client Target client (nullptr = broadcast to all)
     * @return true if frame was sent successfully
     */
    bool sendValidationFrame(AsyncWebSocket* ws, AsyncWebSocketClient* client = nullptr) {
        if (!m_frameReady || m_frameSize == 0) {
            return false;
        }

        if (client) {
            // Send to specific client
            client->binary(m_frameBuffer, m_frameSize);
        } else if (ws) {
            // Broadcast to all clients
            ws->binaryAll(m_frameBuffer, m_frameSize);
        } else {
            return false;
        }

        clearFrame();
        return true;
    }
#endif

private:
    // Ring buffer access via type-erased function pointers
    void* m_ring;
    size_t (*m_ringDrain)(void*, EffectValidationSample*, size_t);
    bool (*m_ringEmpty)(void*);
    size_t (*m_ringAvailable)(void*);

    // Timing
    uint32_t m_drainIntervalMs;
    uint32_t m_lastDrainTime;

    // Frame state
    bool m_frameReady;
    size_t m_frameSize;
    uint16_t m_sampleCount;

    // Frame buffer (no dynamic allocation)
    uint8_t m_frameBuffer[ValidationStreamConfig::MAX_FRAME_SIZE];

    // Temporary sample buffer for drain operation
    EffectValidationSample m_sampleBuffer[ValidationStreamConfig::MAX_SAMPLES_PER_FRAME];

    /**
     * @brief Encode samples into binary frame
     */
    void encodeFrame() {
        using namespace ValidationStreamConfig;

        // Drain up to MAX_SAMPLES_PER_FRAME samples
        size_t drained = m_ringDrain(m_ring, m_sampleBuffer, MAX_SAMPLES_PER_FRAME);

        if (drained == 0) {
            m_frameReady = false;
            return;
        }

        m_sampleCount = static_cast<uint16_t>(drained);
        m_frameSize = HEADER_SIZE + (drained * SAMPLE_SIZE);

        // Write header
        writeHeader();

        // Copy samples to frame buffer
        for (size_t i = 0; i < drained; ++i) {
            std::memcpy(
                m_frameBuffer + HEADER_SIZE + (i * SAMPLE_SIZE),
                &m_sampleBuffer[i],
                SAMPLE_SIZE
            );
        }

        m_frameReady = true;
    }

    /**
     * @brief Write frame header
     *
     * Header format (4 bytes):
     * - Byte 0: 0x54 ('T')
     * - Byte 1: 0x56 ('V')
     * - Byte 2: 0x56 ('V')
     * - Byte 3: Sample count (0-16)
     *
     * The first 3 bytes form a recognizable magic pattern.
     * Combined with sample count = 0, yields 0x00565654 as uint32_t.
     */
    void writeHeader() {
        using namespace ValidationStreamConfig;

        m_frameBuffer[HEADER_OFF_MAGIC_0] = MAGIC_BYTE_0;       // 0x54 'T'
        m_frameBuffer[HEADER_OFF_MAGIC_1] = MAGIC_BYTE_1;       // 0x56 'V'
        m_frameBuffer[HEADER_OFF_MAGIC_2] = MAGIC_BYTE_2;       // 0x56 'V'
        m_frameBuffer[HEADER_OFF_SAMPLE_COUNT] = static_cast<uint8_t>(m_sampleCount & 0xFF);
    }
};

//=============================================================================
// Helper functions for WebSocket integration
//=============================================================================

#if defined(ESP32)
/**
 * @brief Send validation frame via WebSocket (convenience function)
 * @param encoder Frame encoder with ready frame
 * @param ws WebSocket server instance
 * @param client Target client (nullptr = broadcast to all)
 * @return true if frame was sent
 */
inline bool sendValidationFrame(
    ValidationFrameEncoder& encoder,
    AsyncWebSocket* ws,
    AsyncWebSocketClient* client = nullptr)
{
    return encoder.sendValidationFrame(ws, client);
}
#endif

} // namespace validation
} // namespace lightwaveos

#endif // FEATURE_EFFECT_VALIDATION
