/**
 * @file AudioCapture.cpp
 * @brief I2S audio capture implementation for SPH0645 MEMS microphone
 *
 * Uses ESP-IDF legacy I2S driver (driver/i2s.h) for Arduino compatibility.
 *
 * SPH0645 Sample Format:
 * - Outputs 24-bit 2's complement, MSB-first, 18-bit effective precision
 * - I2S configured for 32-bit samples
 * - Conversion: int16_t sample = (int16_t)(raw >> 16)
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "AudioCapture.h"

#if FEATURE_AUDIO_SYNC

#include <cstring>
#include <esp_log.h>

static const char* TAG = "AudioCapture";

namespace lightwaveos {
namespace audio {

// ============================================================================
// Constructor / Destructor
// ============================================================================

AudioCapture::AudioCapture()
    : m_initialized(false)
{
    m_stats.reset();
    memset(m_dmaBuffer, 0, sizeof(m_dmaBuffer));
}

AudioCapture::~AudioCapture()
{
    deinit();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool AudioCapture::init()
{
    if (m_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing I2S for SPH0645 at %d Hz, %d-bit samples",
             SAMPLE_RATE, I2S_BITS_PER_SAMPLE);

    // Configure I2S driver
    if (!configureI2S()) {
        ESP_LOGE(TAG, "Failed to configure I2S driver");
        return false;
    }

    // Set pin configuration
    if (!configurePins()) {
        ESP_LOGE(TAG, "Failed to configure I2S pins");
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    // Start I2S
    esp_err_t err = i2s_start(I2S_PORT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start I2S: %s", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    m_initialized = true;
    ESP_LOGI(TAG, "I2S initialized successfully");
    ESP_LOGI(TAG, "  Sample rate: %d Hz", SAMPLE_RATE);
    ESP_LOGI(TAG, "  Hop size: %d samples (%.1f ms)", HOP_SIZE, HOP_DURATION_MS);
    ESP_LOGI(TAG, "  DMA: %d buffers x %d samples", DMA_BUFFER_COUNT, DMA_BUFFER_SAMPLES);

    return true;
}

void AudioCapture::deinit()
{
    if (!m_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing I2S");

    // Stop I2S
    esp_err_t err = i2s_stop(I2S_PORT);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop I2S: %s", esp_err_to_name(err));
    }

    // Uninstall driver
    err = i2s_driver_uninstall(I2S_PORT);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to uninstall I2S driver: %s", esp_err_to_name(err));
    }

    m_initialized = false;
    ESP_LOGI(TAG, "I2S deinitialized");
}

// ============================================================================
// Audio Capture
// ============================================================================

CaptureResult AudioCapture::captureHop(int16_t* buffer)
{
    if (!m_initialized) {
        return CaptureResult::NOT_INITIALIZED;
    }

    if (buffer == nullptr) {
        return CaptureResult::READ_ERROR;
    }

    // Calculate expected bytes: HOP_SIZE samples x 4 bytes/sample
    const size_t expectedBytes = HOP_SIZE * sizeof(int32_t);
    size_t bytesRead = 0;

    // Record start time for statistics
    uint32_t startTime = esp_log_timestamp();

    // Read from I2S DMA buffer
    // Timeout: 2x hop duration to allow some slack
    const TickType_t timeout = pdMS_TO_TICKS(static_cast<uint32_t>(HOP_DURATION_MS * 2));

    esp_err_t err = i2s_read(I2S_PORT, m_dmaBuffer, expectedBytes, &bytesRead, timeout);

    uint32_t endTime = esp_log_timestamp();
    uint32_t readTimeUs = (endTime - startTime) * 1000;  // Convert ms to us

    // Update timing statistics
    if (readTimeUs > m_stats.maxReadTimeUs) {
        m_stats.maxReadTimeUs = readTimeUs;
    }
    // Simple rolling average
    m_stats.avgReadTimeUs = (m_stats.avgReadTimeUs * 7 + readTimeUs) / 8;

    // Handle errors
    if (err == ESP_ERR_TIMEOUT) {
        m_stats.dmaTimeouts++;
        ESP_LOGW(TAG, "DMA timeout after %lu ms", (unsigned long)(endTime - startTime));
        return CaptureResult::DMA_TIMEOUT;
    }

    if (err != ESP_OK) {
        m_stats.readErrors++;
        ESP_LOGE(TAG, "I2S read error: %s", esp_err_to_name(err));
        return CaptureResult::READ_ERROR;
    }

    // Verify we got the expected amount
    const size_t samplesRead = bytesRead / sizeof(int32_t);
    if (samplesRead < HOP_SIZE) {
        // Partial read - this shouldn't happen with proper DMA config
        ESP_LOGW(TAG, "Partial read: %d/%d samples", samplesRead, HOP_SIZE);
        // Zero-pad remaining samples
        memset(&buffer[samplesRead], 0, (HOP_SIZE - samplesRead) * sizeof(int16_t));
    }

    // Convert 32-bit I2S samples to 16-bit signed
    // SPH0645 format: 24-bit 2's complement in upper 24 bits of 32-bit word
    // Shift right by 16 to get signed 16-bit sample
    int16_t peak = 0;
    for (size_t i = 0; i < HOP_SIZE; i++) {
        // Extract 16-bit sample from 32-bit slot
        int16_t sample = static_cast<int16_t>(m_dmaBuffer[i] >> SAMPLE_SHIFT_BITS);
        buffer[i] = sample;

        // Track peak for level metering
        int16_t absSample = (sample < 0) ? -sample : sample;
        if (absSample > peak) {
            peak = absSample;
        }
    }

    // Update statistics
    m_stats.hopsCapured++;
    m_stats.peakSample = peak;

    return CaptureResult::SUCCESS;
}

// ============================================================================
// Internal Methods
// ============================================================================

bool AudioCapture::configureI2S()
{
    // I2S driver configuration for SPH0645
    i2s_config_t i2sConfig = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,  // 32-bit slots for SPH0645
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,   // SEL tied to GND = left channel
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,  // Standard I2S (Philips)
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUFFER_COUNT,
        .dma_buf_len = DMA_BUFFER_SAMPLES,
        .use_apll = false,                             // Use PLL clock
        .tx_desc_auto_clear = false,                   // RX only
        .fixed_mclk = 0,                               // No fixed MCLK
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,        // MCLK = 256 * Fs
        .bits_per_chan = I2S_BITS_PER_CHAN_32BIT,      // 32-bit channel width
#if SOC_I2S_SUPPORTS_TDM
        .chan_mask = I2S_TDM_ACTIVE_CH0,               // Only channel 0
        .total_chan = 1,                               // Mono
        .left_align = true,                            // Left-aligned data
        .big_edin = false,                             // Little endian
        .bit_order_msb = true,                         // MSB first
        .skip_msk = false,                             // Don't skip any data
#endif
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2sConfig, 0, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2S driver: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "I2S driver installed");
    return true;
}

bool AudioCapture::configurePins()
{
    i2s_pin_config_t pinConfig = {
        .mck_io_num = I2S_PIN_NO_CHANGE,  // SPH0645 doesn't need MCLK
        .bck_io_num = static_cast<int>(I2S_BCLK_PIN),   // GPIO 14 - Bit Clock
        .ws_io_num = static_cast<int>(I2S_LRCL_PIN),    // GPIO 12 - Word Select (LRCLK)
        .data_out_num = I2S_PIN_NO_CHANGE,              // Not transmitting
        .data_in_num = static_cast<int>(I2S_DOUT_PIN),  // GPIO 13 - Data from mic
    };

    esp_err_t err = i2s_set_pin(I2S_PORT, &pinConfig);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set I2S pins: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "I2S pins configured:");
    ESP_LOGI(TAG, "  BCLK: GPIO %d", I2S_BCLK_PIN);
    ESP_LOGI(TAG, "  WS:   GPIO %d", I2S_LRCL_PIN);
    ESP_LOGI(TAG, "  DIN:  GPIO %d", I2S_DOUT_PIN);

    return true;
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
