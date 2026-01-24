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

// ESP-IDF legacy I2S driver
#include "driver/i2s.h"

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

    const CaptureStats& getStats() const { return m_stats; }
    void resetStats() { m_stats.reset(); }

private:
    bool m_initialized;
    static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
    CaptureStats m_stats;
    int32_t m_dmaBuffer[HOP_SIZE * 2];  // 2x for stereo

    // DC-blocking filter state (per-sample IIR high-pass)
    float m_dcPrevInput = 0.0f;
    float m_dcPrevOutput = 0.0f;

    bool configureI2S();
};

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
