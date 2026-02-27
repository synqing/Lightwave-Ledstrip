// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file IAudioCapture.h
 * @brief Hardware abstraction interface for audio capture
 *
 * Provides a platform-agnostic interface for I2S audio capture,
 * supporting both ESP32-S3 (legacy driver) and ESP32-P4 (std mode driver).
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace lightwaveos {
namespace hal {

/**
 * @brief Audio capture statistics
 */
struct CaptureStats {
    uint32_t successCount = 0;      ///< Successful captures
    uint32_t failCount = 0;         ///< Failed captures
    uint32_t overrunCount = 0;      ///< Buffer overruns
    uint32_t lastCaptureUs = 0;     ///< Last capture duration in microseconds
    float dcEstimate = 0.0f;        ///< Estimated DC offset
    float noiseFloor = 0.0f;        ///< Estimated noise floor
};

/**
 * @brief Result of a capture operation
 */
enum class CaptureResult {
    Success,        ///< Samples captured successfully
    Timeout,        ///< DMA timeout
    BufferOverrun,  ///< Samples dropped
    NotInitialized, ///< Driver not initialized
    Error           ///< Generic error
};

/**
 * @brief Audio capture configuration
 */
struct AudioCaptureConfig {
    uint32_t sampleRate = 12800;    ///< Sample rate in Hz
    uint16_t hopSize = 256;         ///< Samples per hop
    uint8_t bclkPin = 14;           ///< I2S bit clock pin
    uint8_t doutPin = 13;           ///< I2S data out pin (mic output)
    uint8_t lrclPin = 12;           ///< I2S left/right clock pin
    uint8_t dmaBufferCount = 4;     ///< DMA buffer count
    uint16_t dmaBufferSize = 512;   ///< DMA buffer size in samples
};

/**
 * @brief Abstract interface for audio capture
 *
 * Platform-specific implementations:
 * - ESP32-S3: AudioCapture_S3 (legacy I2S driver)
 * - ESP32-P4: AudioCapture_P4 (I2S std mode driver)
 */
class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    /**
     * @brief Initialize the audio capture hardware
     * @param config Configuration parameters
     * @return true if initialization succeeded
     */
    virtual bool init(const AudioCaptureConfig& config) = 0;

    /**
     * @brief Deinitialize and release hardware resources
     */
    virtual void deinit() = 0;

    /**
     * @brief Capture one hop of audio samples
     * @param buffer Output buffer for samples (must be hopSize * sizeof(int16_t))
     * @param timeoutMs Maximum time to wait for samples
     * @return Capture result
     */
    virtual CaptureResult captureHop(int16_t* buffer, uint32_t timeoutMs = 100) = 0;

    /**
     * @brief Check if the driver is initialized
     * @return true if initialized and ready
     */
    virtual bool isInitialized() const = 0;

    /**
     * @brief Get capture statistics
     * @return Current statistics
     */
    virtual const CaptureStats& getStats() const = 0;

    /**
     * @brief Reset statistics counters
     */
    virtual void resetStats() = 0;

    /**
     * @brief Get the configured sample rate
     * @return Sample rate in Hz
     */
    virtual uint32_t getSampleRate() const = 0;

    /**
     * @brief Get the configured hop size
     * @return Hop size in samples
     */
    virtual uint16_t getHopSize() const = 0;
};

} // namespace hal
} // namespace lightwaveos
