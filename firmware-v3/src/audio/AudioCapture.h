/**
 * @file AudioCapture.h
 * @brief I2S audio capture driver for SPH0645 MEMS microphone
 *
 * Uses ESP-IDF legacy I2S driver with correct bit alignment.
 * Legacy driver uses >>10 shift (not >>14 like NEW driver).
 *
 * @version 4.1.0 - Legacy driver with correct >>10 bit shift
 */

#pragma once

#include "../config/features.h"

#if FEATURE_AUDIO_SYNC

#include "../config/audio_config.h"
#include <cstdint>

// ESP-IDF I2S driver selection
#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
#include "driver/i2s_std.h"
#include "driver/i2c.h"
#include "es8311.h"
#else
#include "driver/i2s.h"
#endif

namespace lightwaveos {
namespace audio {

enum class CaptureResult : uint8_t {
    SUCCESS = 0,
    NOT_INITIALIZED,
    DMA_TIMEOUT,
    READ_ERROR,
    BUFFER_OVERFLOW
};

struct CaptureStats {
    uint32_t hopsCapured;
    uint32_t dmaTimeouts;
    uint32_t readErrors;
    uint32_t maxReadTimeUs;
    uint32_t avgReadTimeUs;
    int16_t  peakSample;

    void reset() {
        hopsCapured = 0;
        dmaTimeouts = 0;
        readErrors = 0;
        maxReadTimeUs = 0;
        avgReadTimeUs = 0;
        peakSample = 0;
    }
};

class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    bool init();
    void deinit();
    bool isInitialized() const { return m_initialized; }

    CaptureResult captureHop(int16_t* buffer);
    
    /**
     * @brief Non-blocking capture attempt for backlog draining.
     * 
     * Attempts to read a hop with timeout=0. Returns SUCCESS if data was
     * immediately available, DMA_TIMEOUT if no data buffered. This allows
     * the audio actor to drain any backlog accumulated while it was sleeping.
     * 
     * @param buffer Output buffer for HOP_SIZE samples
     * @return SUCCESS if hop captured, DMA_TIMEOUT if nothing buffered
     */
    CaptureResult captureHopNonBlocking(int16_t* buffer);
    
    /**
     * @brief Capture with configurable timeout for multi-hop reading.
     * 
     * Used to read additional hops within a single tick. Short timeout (1-3ms)
     * allows catching up when behind without blocking too long.
     * 
     * @param buffer Output buffer for HOP_SIZE samples
     * @param timeoutMs Timeout in milliseconds (use 1-3ms for catch-up reads)
     * @return SUCCESS if hop captured, DMA_TIMEOUT if timeout elapsed
     */
    CaptureResult captureHopWithTimeout(int16_t* buffer, uint32_t timeoutMs);

    const CaptureStats& getStats() const { return m_stats; }
    void resetStats() { m_stats.reset(); }

#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    /**
     * @brief Get current microphone input gain level.
     * @return Gain in dB (0, 6, 12, 18, 24, 30, 36, 42), or -1 if not supported/initialized
     */
    int8_t getMicGainDb() const { return m_micGainDb; }

    /**
     * @brief Set microphone input gain level.
     * 
     * Valid values: 0, 6, 12, 18, 24, 30, 36, 42 dB
     * This controls the ES8311 codec's ADC gain stage.
     * 
     * @param gainDb Gain in dB (must be multiple of 6, 0-42)
     * @return true if gain was set successfully
     */
    bool setMicGainDb(int8_t gainDb);
#endif

private:
    bool m_initialized;
    CaptureStats m_stats;

#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    i2s_chan_handle_t m_rxChannel;
    int16_t m_dmaBuffer[HOP_SIZE];
    es8311_handle_t m_es8311Handle = nullptr;  // ES8311 codec handle for runtime gain control
    int8_t m_micGainDb = 24;                   // Current mic gain in dB (default 24dB)
#else
    static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
    int32_t m_dmaBuffer[HOP_SIZE * 2];  // 2x for stereo
#endif

    // DC-blocking filter state (per-sample IIR high-pass)
    float m_dcPrevInput = 0.0f;
    float m_dcPrevOutput = 0.0f;

    bool configureI2S();
};

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
