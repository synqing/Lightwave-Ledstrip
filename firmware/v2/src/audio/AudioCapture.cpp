/**
 * @file AudioCapture.cpp
 * @brief I2S audio capture with LEGACY driver and compile-time mic selection
 *
 * Supports mic-type selection via MicType enum in audio_config.h:
 * - SPH0645 (default): 18-bit, RIGHT channel, >>10 shift (K1 hardware)
 * - INMP441: 24-bit, LEFT channel, >>8 shift, MSB_SHIFT set
 *
 * @version 5.0.0 - Compile-time mic-type branching
 */

#include "AudioCapture.h"

#if FEATURE_AUDIO_SYNC

#include <cstring>
#include <cmath>
#include <algorithm>
#include <esp_timer.h>

#include "config/chip_config.h"

#define LW_LOG_TAG "AudioCapture"
#include "utils/Log.h"
#include "AudioDebugConfig.h"

#if !(defined(CHIP_ESP32_P4) && CHIP_ESP32_P4)
#if !(defined(CHIP_ESP32_P4) && CHIP_ESP32_P4)
#include "soc/i2s_reg.h"
#endif
#endif

namespace lightwaveos {
namespace audio {

static constexpr float RECIP_SCALE = 1.0f / 131072.0f;

// First-order DC blocker / high-pass filter coefficient
// Using canonical formula: R = exp(-2*pi*fc/fs)
// fc = 20 Hz (cutoff), fs = 16000 Hz (sample rate)
// R = exp(-2 * 3.14159265 * 20 / 16000) = exp(-0.00785398) ≈ 0.992176
// This removes DC offset and very low frequency drift while preserving audio content
static constexpr float DC_BLOCK_ALPHA = 0.992176f;

AudioCapture::AudioCapture()
    : m_initialized(false)
    , m_dcPrevInput(0.0f)
    , m_dcPrevOutput(0.0f)
{
    m_stats.reset();
    memset(m_dmaBuffer, 0, sizeof(m_dmaBuffer));
#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    m_rxChannel = nullptr;
#endif
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

#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    LW_LOGI("Initializing I2S (std driver, ESP32-P4)");
#else
    LW_LOGI("Initializing I2S (legacy driver with >>10 shift)");
#endif

    if (!configureI2S()) {
        LW_LOGE("Failed to configure I2S driver");
        return false;
    }

#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    esp_err_t err = i2s_channel_enable(m_rxChannel);
    if (err != ESP_OK) {
        LW_LOGE("Failed to enable I2S: %s", esp_err_to_name(err));
        return false;
    }
#else
    esp_err_t err = i2s_start(I2S_PORT);
    if (err != ESP_OK) {
        LW_LOGE("Failed to start I2S: %s", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }
#endif

    m_initialized = true;
#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    LW_LOGI("I2S initialized (std driver, ES8311)");
#else
    LW_LOGI("I2S initialized (legacy driver, >>10 shift)");
#endif
    LW_LOGI("  Sample rate: %d Hz", SAMPLE_RATE);
    LW_LOGI("  Hop size: %d samples (%.1f ms)", HOP_SIZE, HOP_DURATION_MS);
    LW_LOGI("  Pins: BCLK=%d WS=%d DIN=%d", I2S_BCLK_PIN, I2S_LRCL_PIN, I2S_DIN_PIN);

    return true;
}

void AudioCapture::deinit()
{
    if (!m_initialized) return;
    LW_LOGI("Deinitializing I2S");
#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    if (m_rxChannel != nullptr) {
        i2s_channel_disable(m_rxChannel);
        i2s_del_channel(m_rxChannel);
        m_rxChannel = nullptr;
    }
#else
    i2s_stop(I2S_PORT);
    i2s_driver_uninstall(I2S_PORT);
#endif
    m_initialized = false;
}

CaptureResult AudioCapture::captureHop(int16_t* buffer)
{
    if (!m_initialized) return CaptureResult::NOT_INITIALIZED;
    if (buffer == nullptr) return CaptureResult::READ_ERROR;

#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    const size_t expectedBytes = HOP_SIZE * sizeof(int16_t);
    size_t bytesRead = 0;

    uint64_t startTime = esp_timer_get_time();

    const TickType_t timeout = pdMS_TO_TICKS(static_cast<uint32_t>(HOP_DURATION_MS * 2));
    
    // DEBUG: Log first few captures to track I2S read behavior
    static uint32_t s_captureDbg = 0;
    s_captureDbg++;
    if (s_captureDbg <= 5) {
        LW_LOGI("I2S read #%lu: expecting %u bytes, timeout=%lu ticks", 
                (unsigned long)s_captureDbg, (unsigned)expectedBytes, (unsigned long)timeout);
    }
    
    esp_err_t err = i2s_channel_read(m_rxChannel, m_dmaBuffer, expectedBytes, &bytesRead, timeout);

    uint64_t endTime = esp_timer_get_time();
    
    // DEBUG: Log result of first few captures
    if (s_captureDbg <= 5) {
        LW_LOGI("I2S read #%lu: result=%s, got %u bytes, took %lu us",
                (unsigned long)s_captureDbg, esp_err_to_name(err), 
                (unsigned)bytesRead, (unsigned long)(endTime - startTime));
    }
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

    const size_t samplesRead = bytesRead / sizeof(int16_t);
    if (samplesRead < HOP_SIZE) {
        memset(&buffer[samplesRead], 0, (HOP_SIZE - samplesRead) * sizeof(int16_t));
    }

    int16_t peak = 0;
    for (size_t i = 0; i < HOP_SIZE; i++) {
        int16_t rawSample = m_dmaBuffer[i];

        float input = static_cast<float>(rawSample);

        // DC-blocking high-pass filter
        float dcBlocked = input - m_dcPrevInput + DC_BLOCK_ALPHA * m_dcPrevOutput;
        m_dcPrevInput = input;
        m_dcPrevOutput = dcBlocked;

        float clamped = std::max(-32768.0f, std::min(32767.0f, dcBlocked));
        int16_t sample = static_cast<int16_t>(clamped);

        buffer[i] = sample;

        int16_t absSample = (sample < 0) ? -sample : sample;
        if (absSample > peak) peak = absSample;
    }

    m_stats.hopsCapured++;
    m_stats.peakSample = peak;

    return CaptureResult::SUCCESS;
#else
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
        // [DIAG A1] Log BOTH channels with BOTH shift values to identify correct mic config
        int32_t leftMin = INT32_MAX, leftMax = INT32_MIN;
        int32_t rightMin = INT32_MAX, rightMax = INT32_MIN;
        for (size_t i = 0; i < HOP_SIZE; i++) {
            int32_t left  = m_dmaBuffer[i * 2];      // LEFT channel (even indices)
            int32_t right = m_dmaBuffer[i * 2 + 1];  // RIGHT channel (odd indices)
            if (left < leftMin)   leftMin = left;
            if (left > leftMax)   leftMax = left;
            if (right < rightMin) rightMin = right;
            if (right > rightMax) rightMax = right;
        }
        LW_LOGD(LW_CLR_YELLOW "[DIAG-A1] hop=%lu" LW_ANSI_RESET, (unsigned long)s_dbgHop);
        LW_LOGD("  LEFT  raw=[%08X..%08X] >>8=[%ld..%ld] >>10=[%ld..%ld]",
                (uint32_t)leftMin, (uint32_t)leftMax,
                (long)(leftMin >> 8), (long)(leftMax >> 8),
                (long)(leftMin >> 10), (long)(leftMax >> 10));
        LW_LOGD("  RIGHT raw=[%08X..%08X] >>8=[%ld..%ld] >>10=[%ld..%ld]",
                (uint32_t)rightMin, (uint32_t)rightMax,
                (long)(rightMin >> 8), (long)(rightMax >> 8),
                (long)(rightMin >> 10), (long)(rightMax >> 10));
    }

    // =========================================================================
    // SAMPLE CONVERSION WITH DC-BLOCKING FILTER
    // =========================================================================
    // 1. Extract sample from correct channel with mic-appropriate bit shift
    // 2. DC-blocking high-pass filter (removes DC drift, no magic bias needed)
    // 3. Clamp and normalize to int16
    //
    // SPH0645: RIGHT channel (odd DMA indices), >>10 shift (18-bit data)
    // INMP441: LEFT channel (even DMA indices), >>8 shift (24-bit data)
    // =========================================================================
    constexpr size_t CHANNEL_OFFSET = (MICROPHONE_TYPE == MicType::INMP441) ? 0 : 1;
    constexpr int    BIT_SHIFT      = (MICROPHONE_TYPE == MicType::INMP441) ? 8 : 10;

    int16_t peak = 0;
    for (size_t i = 0; i < HOP_SIZE; i++) {
        int32_t rawSample = m_dmaBuffer[i * 2 + CHANNEL_OFFSET];

        // Step 1: Bit shift (mic-type dependent)
        float input = static_cast<float>(rawSample >> BIT_SHIFT);

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
#endif
}

CaptureResult AudioCapture::captureHopNonBlocking(int16_t* buffer)
{
    if (!m_initialized) return CaptureResult::NOT_INITIALIZED;
    if (buffer == nullptr) return CaptureResult::READ_ERROR;

#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    const size_t expectedBytes = HOP_SIZE * sizeof(int16_t);
    size_t bytesRead = 0;

    // Non-blocking read: timeout = 0
    esp_err_t err = i2s_channel_read(m_rxChannel, m_dmaBuffer, expectedBytes, &bytesRead, 0);

    if (err == ESP_ERR_TIMEOUT || bytesRead < expectedBytes) {
        // No data immediately available - this is expected for non-blocking
        return CaptureResult::DMA_TIMEOUT;
    }

    if (err != ESP_OK) {
        m_stats.readErrors++;
        return CaptureResult::READ_ERROR;
    }

    // Process samples (same as blocking version)
    int16_t peak = 0;
    for (size_t i = 0; i < HOP_SIZE; i++) {
        int16_t rawSample = m_dmaBuffer[i];
        float input = static_cast<float>(rawSample);

        // DC-blocking high-pass filter
        float dcBlocked = input - m_dcPrevInput + DC_BLOCK_ALPHA * m_dcPrevOutput;
        m_dcPrevInput = input;
        m_dcPrevOutput = dcBlocked;

        float clamped = std::max(-32768.0f, std::min(32767.0f, dcBlocked));
        buffer[i] = static_cast<int16_t>(clamped);

        int16_t absSample = (buffer[i] < 0) ? -buffer[i] : buffer[i];
        if (absSample > peak) peak = absSample;
    }

    m_stats.hopsCapured++;
    m_stats.peakSample = peak;
    return CaptureResult::SUCCESS;

#else
    // ESP32-S3: Non-blocking read with legacy driver
    const size_t expectedBytes = HOP_SIZE * 2 * sizeof(int32_t);
    size_t bytesRead = 0;

    esp_err_t err = i2s_read(I2S_PORT, m_dmaBuffer, expectedBytes, &bytesRead, 0);

    if (err == ESP_ERR_TIMEOUT || bytesRead < expectedBytes) {
        return CaptureResult::DMA_TIMEOUT;
    }

    if (err != ESP_OK) {
        m_stats.readErrors++;
        return CaptureResult::READ_ERROR;
    }

    // Process samples (same channel extraction and DC blocking as blocking version)
    constexpr size_t CHANNEL_OFFSET = (MICROPHONE_TYPE == MicType::INMP441) ? 0 : 1;
    constexpr int BIT_SHIFT = (MICROPHONE_TYPE == MicType::INMP441) ? 8 : 10;

    int16_t peak = 0;
    for (size_t i = 0; i < HOP_SIZE; i++) {
        int32_t rawSample = m_dmaBuffer[i * 2 + CHANNEL_OFFSET];
        float input = static_cast<float>(rawSample >> BIT_SHIFT);

        float dcBlocked = input - m_dcPrevInput + DC_BLOCK_ALPHA * m_dcPrevOutput;
        m_dcPrevInput = input;
        m_dcPrevOutput = dcBlocked;

        float clamped = std::max(-131072.0f, std::min(131072.0f, dcBlocked));
        float normalized = clamped * RECIP_SCALE;
        buffer[i] = static_cast<int16_t>(normalized * 32767.0f);

        int16_t absSample = (buffer[i] < 0) ? -buffer[i] : buffer[i];
        if (absSample > peak) peak = absSample;
    }

    m_stats.hopsCapured++;
    m_stats.peakSample = peak;
    return CaptureResult::SUCCESS;
#endif
}

CaptureResult AudioCapture::captureHopWithTimeout(int16_t* buffer, uint32_t timeoutMs)
{
    if (!m_initialized) return CaptureResult::NOT_INITIALIZED;
    if (buffer == nullptr) return CaptureResult::READ_ERROR;

#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    const size_t expectedBytes = HOP_SIZE * sizeof(int16_t);
    size_t bytesRead = 0;

    // Convert ms to FreeRTOS ticks (ceiling division, minimum 1)
    TickType_t ticks = (timeoutMs + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS;
    if (ticks == 0) ticks = 1;

    esp_err_t err = i2s_channel_read(m_rxChannel, m_dmaBuffer, expectedBytes, &bytesRead, ticks);

    if (err == ESP_ERR_TIMEOUT || bytesRead < expectedBytes) {
        return CaptureResult::DMA_TIMEOUT;
    }

    if (err != ESP_OK) {
        m_stats.readErrors++;
        return CaptureResult::READ_ERROR;
    }

    // Process samples (same as blocking version)
    int16_t peak = 0;
    for (size_t i = 0; i < HOP_SIZE; i++) {
        int16_t rawSample = m_dmaBuffer[i];
        float input = static_cast<float>(rawSample);

        // DC-blocking high-pass filter
        float dcBlocked = input - m_dcPrevInput + DC_BLOCK_ALPHA * m_dcPrevOutput;
        m_dcPrevInput = input;
        m_dcPrevOutput = dcBlocked;

        float clamped = std::max(-32768.0f, std::min(32767.0f, dcBlocked));
        buffer[i] = static_cast<int16_t>(clamped);

        int16_t absSample = (buffer[i] < 0) ? -buffer[i] : buffer[i];
        if (absSample > peak) peak = absSample;
    }

    m_stats.hopsCapured++;
    m_stats.peakSample = peak;
    return CaptureResult::SUCCESS;

#else
    // ESP32-S3: Short timeout read with legacy driver
    const size_t expectedBytes = HOP_SIZE * 2 * sizeof(int32_t);
    size_t bytesRead = 0;

    TickType_t ticks = (timeoutMs + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS;
    if (ticks == 0) ticks = 1;

    esp_err_t err = i2s_read(I2S_PORT, m_dmaBuffer, expectedBytes, &bytesRead, ticks);

    if (err == ESP_ERR_TIMEOUT || bytesRead < expectedBytes) {
        return CaptureResult::DMA_TIMEOUT;
    }

    if (err != ESP_OK) {
        m_stats.readErrors++;
        return CaptureResult::READ_ERROR;
    }

    // Process samples (same channel extraction and DC blocking as blocking version)
    constexpr size_t CHANNEL_OFFSET = (MICROPHONE_TYPE == MicType::INMP441) ? 0 : 1;
    constexpr int BIT_SHIFT = (MICROPHONE_TYPE == MicType::INMP441) ? 8 : 10;

    int16_t peak = 0;
    for (size_t i = 0; i < HOP_SIZE; i++) {
        int32_t rawSample = m_dmaBuffer[i * 2 + CHANNEL_OFFSET];
        float input = static_cast<float>(rawSample >> BIT_SHIFT);

        float dcBlocked = input - m_dcPrevInput + DC_BLOCK_ALPHA * m_dcPrevOutput;
        m_dcPrevInput = input;
        m_dcPrevOutput = dcBlocked;

        float clamped = std::max(-32768.0f, std::min(32767.0f, dcBlocked));
        buffer[i] = static_cast<int16_t>(clamped);

        int16_t absSample = (buffer[i] < 0) ? -buffer[i] : buffer[i];
        if (absSample > peak) peak = absSample;
    }

    m_stats.hopsCapured++;
    m_stats.peakSample = peak;
    return CaptureResult::SUCCESS;
#endif
}

bool AudioCapture::configureI2S()
{
#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
    // =========================================================================
    // PHASE 1: I2S Channel Creation
    // =========================================================================
    LW_LOGI("=== ESP32-P4 Audio Init ===");
    
    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chanCfg.auto_clear = true;
    chanCfg.dma_frame_num = HOP_SIZE;
    chanCfg.dma_desc_num = 4;

    esp_err_t err = i2s_new_channel(&chanCfg, nullptr, &m_rxChannel);
    if (err != ESP_OK) {
        LW_LOGE("i2s_new_channel failed: %s", esp_err_to_name(err));
        return false;
    }
    LW_LOGI("[1/7] I2S channel created");

    // =========================================================================
    // PHASE 2: I2S Configuration with EXPLICIT clock source
    // On ESP32-P4, I2S_CLK_SRC_DEFAULT uses PLL clock. We use the default
    // config macro but document that APLL may fail if occupied by other
    // peripherals (EMAC, etc.). If clock init fails, this is a likely cause.
    // =========================================================================
    i2s_std_config_t stdCfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = static_cast<gpio_num_t>(chip::gpio::I2S_MCLK),
            .bclk = static_cast<gpio_num_t>(chip::gpio::I2S_BCLK),
            .ws = static_cast<gpio_num_t>(chip::gpio::I2S_LRCL),
            .dout = static_cast<gpio_num_t>(chip::gpio::I2S_DOUT),
            .din = static_cast<gpio_num_t>(chip::gpio::I2S_DIN),
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    stdCfg.clk_cfg.mclk_multiple = static_cast<i2s_mclk_multiple_t>(I2S_MCLK_MULTIPLE);
    stdCfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    LW_LOGI("I2S clock: src=DEFAULT, rate=%u, mclk_mult=%u", 
            SAMPLE_RATE, I2S_MCLK_MULTIPLE);

    err = i2s_channel_init_std_mode(m_rxChannel, &stdCfg);
    if (err != ESP_OK) {
        LW_LOGE("i2s_channel_init_std_mode failed: %s", esp_err_to_name(err));
        i2s_del_channel(m_rxChannel);
        m_rxChannel = nullptr;
        return false;
    }
    LW_LOGI("[2/7] I2S std mode configured");

    // =========================================================================
    // PHASE 3: Audio PA Enable
    // =========================================================================
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << chip::gpio::AUDIO_PA_EN);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(static_cast<gpio_num_t>(chip::gpio::AUDIO_PA_EN), 1);
    LW_LOGI("[3/7] Audio PA enabled (GPIO %d)", chip::gpio::AUDIO_PA_EN);

    // =========================================================================
    // PHASE 4: I2C Bus Setup for ES8311 Control
    // =========================================================================
    i2c_config_t es_i2c_cfg = {};
    es_i2c_cfg.sda_io_num = static_cast<gpio_num_t>(chip::gpio::I2C_SDA);
    es_i2c_cfg.scl_io_num = static_cast<gpio_num_t>(chip::gpio::I2C_SCL);
    es_i2c_cfg.mode = I2C_MODE_MASTER;
    es_i2c_cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    es_i2c_cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    es_i2c_cfg.master.clk_speed = 100000;
    
    err = i2c_param_config(I2C_NUM_0, &es_i2c_cfg);
    if (err != ESP_OK) {
        LW_LOGE("i2c_param_config failed: %s", esp_err_to_name(err));
        return false;
    }
    
    err = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        LW_LOGE("i2c_driver_install failed: %s", esp_err_to_name(err));
        return false;
    }
    LW_LOGI("[4/7] I2C configured: SDA=%d, SCL=%d", 
            chip::gpio::I2C_SDA, chip::gpio::I2C_SCL);

    // =========================================================================
    // PHASE 5: I2C Bus Scan - Find ES8311 Address
    // ES8311 address is determined by CE pin: CE=0 -> 0x18, CE=1 -> 0x19
    // Rather than assume, we probe both addresses.
    // 
    // NOTE: Using i2c_master_cmd_begin with write-only (no data) to check ACK.
    // i2c_master_write_read_device with 0-length read causes errors on some chips.
    // =========================================================================
    LW_LOGI("[5/7] Scanning I2C bus for ES8311...");
    
    const uint8_t ES8311_ADDR_CE_LOW  = 0x18;  // ES8311_ADDRRES_0
    const uint8_t ES8311_ADDR_CE_HIGH = 0x19;  // ES8311_ADDRRES_1
    uint8_t es8311_addr = 0;
    
    // Helper lambda to probe I2C address using low-level cmd API
    auto i2c_probe = [](uint8_t addr) -> esp_err_t {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(10));
        i2c_cmd_link_delete(cmd);
        return ret;
    };
    
    // Probe both possible ES8311 addresses
    for (uint8_t addr : {ES8311_ADDR_CE_LOW, ES8311_ADDR_CE_HIGH}) {
        err = i2c_probe(addr);
        if (err == ESP_OK) {
            LW_LOGI("  ES8311 found at 0x%02X (ACK)", addr);
            es8311_addr = addr;
            break;  // Found it, stop searching
        } else {
            LW_LOGD("  No device at 0x%02X (err=%s)", addr, esp_err_to_name(err));
        }
    }
    
    if (es8311_addr == 0) {
        // Full bus scan for debugging
        LW_LOGW("ES8311 not found at expected addresses. Full I2C scan:");
        int devices_found = 0;
        for (uint8_t addr = 0x08; addr < 0x78; addr++) {
            err = i2c_probe(addr);
            if (err == ESP_OK) {
                LW_LOGI("  Device found at 0x%02X", addr);
                devices_found++;
            }
        }
        if (devices_found == 0) {
            LW_LOGE("No I2C devices found! Check wiring: SDA=%d, SCL=%d", 
                    chip::gpio::I2C_SDA, chip::gpio::I2C_SCL);
        }
        LW_LOGE("ES8311 codec not detected on I2C bus");
        return false;
    }

    // =========================================================================
    // PHASE 6: ES8311 Codec Initialization with Probed Address
    // =========================================================================
    if (m_es8311Handle == nullptr) {
        m_es8311Handle = es8311_create(I2C_NUM_0, es8311_addr);
    }
    if (m_es8311Handle == nullptr) {
        LW_LOGE("es8311_create failed for addr 0x%02X", es8311_addr);
        return false;
    }
    LW_LOGI("[6/7] ES8311 handle created at 0x%02X", es8311_addr);

    const es8311_clock_config_t es_clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = static_cast<int>(SAMPLE_RATE * I2S_MCLK_MULTIPLE),
        .sample_frequency = static_cast<int>(SAMPLE_RATE),
    };
    
    LW_LOGI("ES8311 clock: mclk=%d Hz, sample=%d Hz", 
            es_clk.mclk_frequency, es_clk.sample_frequency);
    
    err = es8311_init(m_es8311Handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
    if (err != ESP_OK) {
        LW_LOGE("es8311_init failed: %s", esp_err_to_name(err));
        return false;
    }
    
    err = es8311_sample_frequency_config(m_es8311Handle, SAMPLE_RATE * I2S_MCLK_MULTIPLE, SAMPLE_RATE);
    if (err != ESP_OK) {
        LW_LOGE("es8311_sample_frequency_config failed: %s", esp_err_to_name(err));
        return false;
    }
    
    err = es8311_microphone_config(m_es8311Handle, false);
    if (err != ESP_OK) {
        LW_LOGE("es8311_microphone_config failed: %s", esp_err_to_name(err));
        return false;
    }

    // Set microphone input gain (runtime tunable via API)
    // Signal levels were observed at ~0.2% of full scale (raw=[-45..64], rms=0.0008)
    // which is ~-54dB below full scale. Default 24dB gain as starting point.
    // Available: 0dB, 6dB, 12dB, 18dB, 24dB, 30dB, 36dB, 42dB
    // Can be changed at runtime via setMicGainDb() / REST API / WebSocket
    if (!setMicGainDb(m_micGainDb)) {
        LW_LOGW("Initial mic gain set failed (continuing with codec default)");
    }

    // =========================================================================
    // PHASE 7: Summary
    // =========================================================================
    LW_LOGI("[7/7] Audio init complete!");
    LW_LOGI("  I2S: DMA=%u samples x %u descs, rate=%u Hz", 
            static_cast<unsigned>(chanCfg.dma_frame_num),
            static_cast<unsigned>(chanCfg.dma_desc_num),
            static_cast<unsigned>(SAMPLE_RATE));
    LW_LOGI("  ES8311: addr=0x%02X, 16-bit, MCLK=%d Hz", 
            es8311_addr, es_clk.mclk_frequency);

    return true;
#else
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

    // Register settings AFTER i2s_set_pin() — mic-type dependent
    if constexpr (MICROPHONE_TYPE == MicType::INMP441) {
        // INMP441: SET MSB_SHIFT to compensate 1-bit I2S delay
        REG_SET_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT);
        REG_CLR_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_WS_IDLE_POL);
        REG_SET_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_LEFT_ALIGN);
        REG_SET_BIT(I2S_RX_TIMING_REG(I2S_PORT), BIT(9));
        LW_LOGI("I2S configured: INMP441, LEFT ch, MSB_SHIFT set, >>8 shift");
    } else {
        // SPH0645: CLEAR MSB_SHIFT, data in RIGHT channel
        REG_CLR_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT);
        REG_CLR_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_WS_IDLE_POL);
        REG_SET_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_LEFT_ALIGN);
        REG_SET_BIT(I2S_RX_TIMING_REG(I2S_PORT), BIT(9));
        LW_LOGI("I2S configured: SPH0645, RIGHT ch, MSB_SHIFT clear, >>10 shift");
    }
    return true;
#endif
}

#if defined(CHIP_ESP32_P4) && CHIP_ESP32_P4
bool AudioCapture::setMicGainDb(int8_t gainDb) {
    if (!m_initialized || m_es8311Handle == nullptr) {
        LW_LOGE("Cannot set mic gain: not initialized");
        return false;
    }

    // Validate and map gainDb to ES8311 gain enum
    // Valid values: 0, 6, 12, 18, 24, 30, 36, 42
    es8311_mic_gain_t gainEnum;
    switch (gainDb) {
        case 0:  gainEnum = ES8311_MIC_GAIN_0DB;  break;
        case 6:  gainEnum = ES8311_MIC_GAIN_6DB;  break;
        case 12: gainEnum = ES8311_MIC_GAIN_12DB; break;
        case 18: gainEnum = ES8311_MIC_GAIN_18DB; break;
        case 24: gainEnum = ES8311_MIC_GAIN_24DB; break;
        case 30: gainEnum = ES8311_MIC_GAIN_30DB; break;
        case 36: gainEnum = ES8311_MIC_GAIN_36DB; break;
        case 42: gainEnum = ES8311_MIC_GAIN_42DB; break;
        default:
            LW_LOGE("Invalid mic gain %ddB (valid: 0,6,12,18,24,30,36,42)", gainDb);
            return false;
    }

    esp_err_t err = es8311_microphone_gain_set(m_es8311Handle, gainEnum);
    if (err != ESP_OK) {
        LW_LOGE("es8311_microphone_gain_set failed: %s", esp_err_to_name(err));
        return false;
    }

    m_micGainDb = gainDb;
    LW_LOGI("Microphone gain set to %ddB", gainDb);
    return true;
}
#endif

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
