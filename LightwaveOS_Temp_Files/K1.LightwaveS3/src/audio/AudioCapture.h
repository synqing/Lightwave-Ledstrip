/**
 * @file AudioCapture.h
 * @brief I2S audio capture driver for SPH0645 MEMS microphone
 *
 * This module provides low-level I2S audio capture using the ESP-IDF legacy
 * I2S driver with configuration corrections.
 *
 * Key features:
 * - Uses ESP-IDF legacy driver (driver/i2s.h) for Arduino compatibility
 * - SPH0645 RIGHT slot on ESP32-S3 (SEL=GND wiring; WS inverted in legacy driver)
 * - DMA-based capture with 4 x 512 sample buffers
 * - 256-sample hop size for Tab5 parity (62.5 Hz frames)
 * - I2S register fixes: MSB shift enabled, timing delay (BIT(9))
 * - Sample conversion: >>14 shift to extract 18-bit data (validate via DMA dbg)
 *
 * Thread Safety:
 * - init()/deinit() must be called from the same task
 * - captureHop() is single-threaded (called only from AudioNode)
 *
 * @author LightwaveOS Team
 * @version 2.1.0
 */

#pragma once

#include "../config/features.h"

#if FEATURE_AUDIO_SYNC

#include "../config/audio_config.h"
#include <cstdint>

// ESP-IDF legacy I2S driver (Arduino compatible)
#include "driver/i2s.h"

namespace lightwaveos {
namespace audio {

/**
 * @brief Capture result codes
 */
enum class CaptureResult : uint8_t {
    SUCCESS = 0,           // Hop captured successfully
    NOT_INITIALIZED,       // init() not called
    DMA_TIMEOUT,           // Timed out waiting for DMA
    READ_ERROR,            // I2S read failed
    BUFFER_OVERFLOW        // Internal buffer overflow
};

/**
 * @brief Capture statistics for diagnostics
 */
struct CaptureStats {
    uint32_t hopsCapured;      // Total hops captured
    uint32_t dmaTimeouts;      // DMA timeout count
    uint32_t readErrors;       // I2S read error count
    uint32_t maxReadTimeUs;    // Maximum read time observed
    uint32_t avgReadTimeUs;    // Rolling average read time
    int16_t  peakSample;       // Peak sample value (for level metering)

    void reset() {
        hopsCapured = 0;
        dmaTimeouts = 0;
        readErrors = 0;
        maxReadTimeUs = 0;
        avgReadTimeUs = 0;
        peakSample = 0;
    }
};

/**
 * @brief I2S audio capture wrapper for SPH0645 MEMS microphone
 *
 * Provides RAII-style initialization and hop-based audio capture.
 * Each hop is 256 samples at 16 kHz = 16 ms of audio.
 *
 * Usage:
 *   AudioCapture capture;
 *   if (capture.init()) {
 *       int16_t buffer[256];
 *       while (running) {
 *           if (capture.captureHop(buffer) == CaptureResult::SUCCESS) {
 *               // Process 256 samples
 *           }
 *       }
 *       capture.deinit();
 *   }
 */
class AudioCapture {
public:
    /**
     * @brief Constructor - does NOT initialize hardware
     *
     * Call init() to start I2S capture.
     */
    AudioCapture();

    /**
     * @brief Destructor - calls deinit() if needed
     */
    ~AudioCapture();

    // Non-copyable (owns hardware resource)
    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * @brief Initialize I2S hardware and DMA
     *
     * Configures:
     * - I2S_NUM_0 in RX-only mode
     * - 16 kHz sample rate
     * - 32-bit samples (mono, left channel)
     * - DMA: 4 buffers x 512 samples
     *
     * @return true if initialization succeeded
     */
    bool init();

    /**
     * @brief Deinitialize I2S hardware
     *
     * Stops I2S and releases DMA resources.
     * Safe to call multiple times.
     */
    void deinit();

    /**
     * @brief Check if I2S is initialized and running
     */
    bool isInitialized() const { return m_initialized; }

    // ========================================================================
    // Audio Capture
    // ========================================================================

    /**
     * @brief Capture one hop of audio samples
     *
     * Blocks until HOP_SIZE (256) samples are available from DMA.
     * Performs 32-bit to 16-bit conversion (>>16 shift).
     *
     * @param buffer Output buffer for 16-bit signed samples
     *               Must be at least HOP_SIZE (256) elements
     * @return CaptureResult indicating success or error type
     */
    CaptureResult captureHop(int16_t* buffer);

    /**
     * @brief Get capture statistics
     */
    const CaptureStats& getStats() const { return m_stats; }

    /**
     * @brief Reset statistics
     */
    void resetStats() { m_stats.reset(); }

private:
    // ========================================================================
    // Internal State
    // ========================================================================

    bool m_initialized;

    // I2S port number (legacy API)
    static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;

    // Statistics
    CaptureStats m_stats;

    // Internal DMA read buffer (32-bit samples)
    // Size = HOP_SIZE * 4 bytes = 1024 bytes
    int32_t m_dmaBuffer[HOP_SIZE];

    // ========================================================================
    // Internal Methods
    // ========================================================================

    /**
     * @brief Configure I2S driver (legacy API)
     *
     * SPH0645 configuration for ESP32-S3:
     * - I2S_CHANNEL_FMT_ONLY_LEFT (SEL=GND on Adafruit breakout)
     * - 32-bit slots for SPH0645 (18-bit data, MSB-aligned)
     * - MSB shift enabled (REG_SET_BIT I2S_RX_MSB_SHIFT)
     * - Timing delay enabled (REG_SET_BIT BIT(9) in timing reg)
     *
     * @return true if configuration succeeded
     */
    bool configureI2S();

    /**
     * @brief Set I2S pin configuration
     * @return true if pin config succeeded
     */
    bool configurePins();
};

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
