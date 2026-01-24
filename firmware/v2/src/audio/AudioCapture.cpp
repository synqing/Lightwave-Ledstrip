/**
 * @file AudioCapture.cpp
 * @brief I2S audio capture with LEGACY driver and CORRECT bit shift
 *
 * CRITICAL FIX: Legacy driver bit alignment is different from NEW driver.
 * - NEW driver (Emotiscope): data in bits [31:14], use >>14
 * - LEGACY driver: data in bits [31:10], use >>10
 *
 * @version 4.1.0 - Legacy driver with >>10 bit shift
 */

#include "AudioCapture.h"

#if FEATURE_AUDIO_SYNC

#include <cstring>
#include <cmath>
#include <algorithm>
#include <esp_timer.h>

#define LW_LOG_TAG "AudioCapture"
#include "utils/Log.h"
#include "AudioDebugConfig.h"

#include "soc/i2s_reg.h"

namespace lightwaveos {
namespace audio {

static constexpr float RECIP_SCALE = 1.0f / 131072.0f;

// DC-blocking high-pass filter coefficient
// alpha = 1 - (2 * pi * fc / fs), fc=10Hz, fs=12800Hz
// Removes DC drift while preserving audio content
static constexpr float DC_BLOCK_ALPHA = 0.9951f;

AudioCapture::AudioCapture()
    : m_initialized(false)
    , m_dcPrevInput(0.0f)
    , m_dcPrevOutput(0.0f)
{
    m_stats.reset();
    memset(m_dmaBuffer, 0, sizeof(m_dmaBuffer));
}

AudioCapture::~AudioCapture()
{
    deinit();
}

bool AudioCapture::init()
{
    if (m_initialized) {
        LW_LOGW("Already initialized");
        return true;
    }

    LW_LOGI("Initializing I2S (legacy driver with >>10 shift)");

    if (!configureI2S()) {
        LW_LOGE("Failed to configure I2S driver");
        return false;
    }

    esp_err_t err = i2s_start(I2S_PORT);
    if (err != ESP_OK) {
        LW_LOGE("Failed to start I2S: %s", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    m_initialized = true;
    LW_LOGI("I2S initialized (legacy driver, >>10 shift)");
    LW_LOGI("  Sample rate: %d Hz", SAMPLE_RATE);
    LW_LOGI("  Hop size: %d samples (%.1f ms)", HOP_SIZE, HOP_DURATION_MS);
    LW_LOGI("  Pins: BCLK=%d WS=%d DIN=%d", I2S_BCLK_PIN, I2S_LRCL_PIN, I2S_DOUT_PIN);

    return true;
}

void AudioCapture::deinit()
{
    if (!m_initialized) return;
    LW_LOGI("Deinitializing I2S");
    i2s_stop(I2S_PORT);
    i2s_driver_uninstall(I2S_PORT);
    m_initialized = false;
}

CaptureResult AudioCapture::captureHop(int16_t* buffer)
{
    if (!m_initialized) return CaptureResult::NOT_INITIALIZED;
    if (buffer == nullptr) return CaptureResult::READ_ERROR;

    const size_t expectedBytes = HOP_SIZE * 2 * sizeof(int32_t);
    size_t bytesRead = 0;

    uint64_t startTime = esp_timer_get_time();

    const TickType_t timeout = pdMS_TO_TICKS(static_cast<uint32_t>(HOP_DURATION_MS * 2));
    esp_err_t err = i2s_read(I2S_PORT, m_dmaBuffer, expectedBytes, &bytesRead, timeout);

    uint64_t endTime = esp_timer_get_time();
    uint32_t readTimeUs = static_cast<uint32_t>(endTime - startTime);

    if (readTimeUs > m_stats.maxReadTimeUs) m_stats.maxReadTimeUs = readTimeUs;
    m_stats.avgReadTimeUs = (m_stats.avgReadTimeUs * 7 + readTimeUs) / 8;

    if (err == ESP_ERR_TIMEOUT) {
        m_stats.dmaTimeouts++;
        return CaptureResult::DMA_TIMEOUT;
    }

    if (err != ESP_OK) {
        m_stats.readErrors++;
        LW_LOGE("I2S read error: %s", esp_err_to_name(err));
        return CaptureResult::READ_ERROR;
    }

    const size_t monoSamplesRead = bytesRead / sizeof(int32_t) / 2;
    if (monoSamplesRead < HOP_SIZE) {
        memset(&buffer[monoSamplesRead], 0, (HOP_SIZE - monoSamplesRead) * sizeof(int16_t));
    }

    // DMA Debug logging - Level 5 (TRACE) only
    // This is deep debugging info, not needed for normal operation
    // Use 'adbg 5' to enable, or 'adbg status' for one-shot info
    static uint32_t s_dbgHop = 0;
    static bool s_firstPrint = true;
    s_dbgHop++;

    auto& dbgCfg = getAudioDebugConfig();
    if (dbgCfg.verbosity >= 5 && (s_firstPrint || (s_dbgHop % dbgCfg.intervalDMA()) == 0)) {
        s_firstPrint = false;
        int32_t rawMin = INT32_MAX, rawMax = INT32_MIN;
        for (size_t i = 1; i < HOP_SIZE * 2; i += 2) {
            int32_t v = m_dmaBuffer[i];
            if (v < rawMin) rawMin = v;
            if (v > rawMax) rawMax = v;
        }
        LW_LOGD(LW_CLR_YELLOW "DMA:" LW_ANSI_RESET " hop=%lu R=[%08X..%08X] >>10=[%ld..%ld]",
                (unsigned long)s_dbgHop,
                (uint32_t)rawMin, (uint32_t)rawMax,
                (long)(rawMin >> 10), (long)(rawMax >> 10));
    }

    // =========================================================================
    // SAMPLE CONVERSION WITH DC-BLOCKING FILTER
    // =========================================================================
    // 1. >>10 shift (legacy driver bit alignment)
    // 2. DC-blocking high-pass filter (removes DC drift, no magic bias needed)
    // 3. Clamp and normalize to int16
    // =========================================================================
    int16_t peak = 0;
    for (size_t i = 0; i < HOP_SIZE; i++) {
        int32_t rawSample = m_dmaBuffer[i * 2 + 1];  // RIGHT channel

        // Step 1: >>10 shift (legacy driver alignment)
        float input = static_cast<float>(rawSample >> 10);

        // Step 2: DC-blocking high-pass filter
        // y[n] = x[n] - x[n-1] + alpha * y[n-1]
        float dcBlocked = input - m_dcPrevInput + DC_BLOCK_ALPHA * m_dcPrevOutput;
        m_dcPrevInput = input;
        m_dcPrevOutput = dcBlocked;

        // Step 3: Clamp to ±131072 range, normalize to [-1, 1], then to int16
        float clamped = std::max(-131072.0f, std::min(131072.0f, dcBlocked));
        float normalized = clamped * RECIP_SCALE;
        int16_t sample = static_cast<int16_t>(normalized * 32767.0f);

        buffer[i] = sample;

        int16_t absSample = (sample < 0) ? -sample : sample;
        if (absSample > peak) peak = absSample;
    }

    m_stats.hopsCapured++;
    m_stats.peakSample = peak;

    return CaptureResult::SUCCESS;
}

bool AudioCapture::configureI2S()
{
    i2s_config_t i2sConfig = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = static_cast<int>(DMA_BUFFER_COUNT),
        .dma_buf_len = static_cast<int>(DMA_BUFFER_SAMPLES * 2),
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan = I2S_BITS_PER_CHAN_32BIT,
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2sConfig, 0, nullptr);
    if (err != ESP_OK) {
        LW_LOGE("Failed to install I2S driver: %s", esp_err_to_name(err));
        return false;
    }

    i2s_pin_config_t pinConfig = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = static_cast<int>(I2S_BCLK_PIN),
        .ws_io_num = static_cast<int>(I2S_LRCL_PIN),
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = static_cast<int>(I2S_DOUT_PIN),
    };

    err = i2s_set_pin(I2S_PORT, &pinConfig);
    if (err != ESP_OK) {
        LW_LOGE("Failed to set I2S pins: %s", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    // Register settings AFTER i2s_set_pin()
    REG_CLR_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT);
    REG_CLR_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_WS_IDLE_POL);  // NO WS inversion - data in RIGHT channel
    REG_SET_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_LEFT_ALIGN);
    REG_SET_BIT(I2S_RX_TIMING_REG(I2S_PORT), BIT(9));

    LW_LOGI("I2S configured: STEREO, MSB format, >>10 shift");
    return true;
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
