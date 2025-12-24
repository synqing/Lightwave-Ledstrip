/**
 * @file AudioCapture.cpp
 * @brief I2S audio capture implementation for SPH0645 MEMS microphone
 *
 * Uses ESP-IDF legacy I2S driver with ESP32-S3 register fixes for SPH0645.
 *
 * SPH0645 Sample Format:
 * - Outputs 18-bit data, MSB-first, in 32-bit I2S slots
 * - I2S configured for 32-bit samples, LEFT channel (SEL=GND)
 * - Register fixes: MSB shift enabled, timing delay (BIT(9))
 * - Conversion: >>14 shift to extract 18-bit value, then scale to 16-bit
 * - DC removal handled in AudioActor
 *
 * @author LightwaveOS Team
 * @version 2.2.0
 */

#include "AudioCapture.h"

#if FEATURE_AUDIO_SYNC

#include <cstring>
#include <cmath>
#include <esp_log.h>
#include <esp_timer.h>

// ESP32-S3 register access for WS polarity fix (SPH0645 requirement)
// The legacy I2S driver doesn't expose WS polarity, so we set the register directly
#include "soc/i2s_reg.h"

static const char* TAG = "AudioCapture";

namespace lightwaveos {
namespace audio {

// Emotiscope-proven sample conversion constants
static constexpr int32_t DC_BIAS_ADD = 7000;       // Pre-clip bias adjustment
static constexpr int32_t DC_BIAS_SUB = 360;        // Post-clip DC removal
static constexpr int32_t CLIP_MAX = 131071;        // 18-bit max value (2^17 - 1)
static constexpr int32_t CLIP_MIN = -131072;       // 18-bit min value

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

    ESP_LOGI(TAG, "Initializing I2S for SPH0645 (RIGHT channel, Emotiscope conversion)");

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
    ESP_LOGI(TAG, "  Pins: BCLK=%d WS=%d DIN=%d", I2S_BCLK_PIN, I2S_LRCL_PIN, I2S_DOUT_PIN);
    ESP_LOGI(TAG, "  Channel: RIGHT");

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

    // Record start time for statistics (microseconds)
    uint64_t startTime = esp_timer_get_time();

    // Read from I2S DMA buffer
    // Timeout: 2x hop duration to allow some slack
    const TickType_t timeout = pdMS_TO_TICKS(static_cast<uint32_t>(HOP_DURATION_MS * 2));

    esp_err_t err = i2s_read(I2S_PORT, m_dmaBuffer, expectedBytes, &bytesRead, timeout);

    uint64_t endTime = esp_timer_get_time();
    uint32_t readTimeUs = static_cast<uint32_t>(endTime - startTime);

    // Update timing statistics
    if (readTimeUs > m_stats.maxReadTimeUs) {
        m_stats.maxReadTimeUs = readTimeUs;
    }
    // Simple rolling average
    m_stats.avgReadTimeUs = (m_stats.avgReadTimeUs * 7 + readTimeUs) / 8;

    // Handle errors
    if (err == ESP_ERR_TIMEOUT) {
        m_stats.dmaTimeouts++;
        ESP_LOGD(TAG, "DMA timeout after %lu us", (unsigned long)readTimeUs);
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
        ESP_LOGW(TAG, "Partial read: %zu/%d samples", samplesRead, HOP_SIZE);
        memset(&buffer[samplesRead], 0, (HOP_SIZE - samplesRead) * sizeof(int16_t));
    }

    static uint32_t s_dbgHop = 0;
    static bool s_firstPrint = true;
    s_dbgHop++;
    
    // Print first time immediately, then every 50 hops (~0.8 seconds) for visibility
    if (s_firstPrint || (s_dbgHop % 50) == 0) {
        s_firstPrint = false;
        int32_t rawMin = INT32_MAX;
        int32_t rawMax = INT32_MIN;
        for (size_t i = 0; i < HOP_SIZE; ++i) {
            int32_t v = m_dmaBuffer[i];
            if (v < rawMin) rawMin = v;
            if (v > rawMax) rawMax = v;
        }

        auto peakShift = [&](int shift) -> int32_t {
            int32_t pk = 0;
            for (size_t i = 0; i < HOP_SIZE; ++i) {
                int32_t s = (m_dmaBuffer[i] >> shift);
                int32_t a = (s < 0) ? -s : s;
                if (a > pk) pk = a;
            }
            return pk;
        };

        // Check MSB shift register state for diagnostic
        bool msbShiftEnabled = (REG_GET_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT) != 0);
        const char* channelFmt = "RIGHT";  // Current config uses RIGHT channel

        // Use ESP_LOGI (same level as "Audio alive") with ANSI color codes (bright cyan)
        ESP_LOGI(TAG,
                 "\033[1;36mDMA dbg: hop=%lu ch=%s msb_shift=%s raw0=%08X raw1=%08X min=%ld max=%ld "
                 "pk>>8=%ld pk>>10=%ld pk>>12=%ld pk>>14=%ld pk>>16=%ld\033[0m",
                 (unsigned long)s_dbgHop, channelFmt, msbShiftEnabled ? "ON" : "OFF",
                 (uint32_t)m_dmaBuffer[0], (uint32_t)m_dmaBuffer[1],
                 (long)rawMin, (long)rawMax,
                 (long)peakShift(8), (long)peakShift(10), (long)peakShift(12), 
                 (long)peakShift(14), (long)peakShift(16));
    }

    // Convert samples from 32-bit I2S slots to 16-bit signed samples
    // SPH0645 outputs 18-bit data, MSB-aligned in 32-bit slot.
    // With MSB shift enabled, hardware aligns data; >>14 typically extracts 18-bit value.
    // NOTE: Validate shift against DMA dbg output (pk>>N values) - the diagnostic
    //       shows which shift produces the largest peak, indicating correct alignment.
    // 1. Shift right by 14 to extract 18-bit value (may need adjustment based on diagnostics)
    // 2. Clip to 18-bit range (safety)
    // 3. Scale to 16-bit (>> 2)
    // 4. DC removal is handled in AudioActor
    
    int16_t peak = 0;
    for (size_t i = 0; i < HOP_SIZE; i++) {
        // Step 1: Shift by 14 to get 18-bit value with sign preserved
        // If DMA dbg shows pk>>12 or pk>>16 is much larger, adjust this shift accordingly
        int32_t raw = (m_dmaBuffer[i] >> 14);
        raw += DC_BIAS_ADD;
        if (raw < CLIP_MIN) raw = CLIP_MIN;
        if (raw > CLIP_MAX) raw = CLIP_MAX;
        raw -= DC_BIAS_SUB;
        int16_t sample = static_cast<int16_t>(raw >> 2);
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
    // ALIGNED WITH K1.LIGHTWAVE: Use LEFT channel and specific register hacks
    i2s_config_t i2sConfig = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,  // 32-bit slots for SPH0645
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,   // SPH0645: RIGHT slot on ESP32-S3
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,  // Standard I2S format
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = static_cast<int>(DMA_BUFFER_COUNT),
        .dma_buf_len = static_cast<int>(DMA_BUFFER_SAMPLES),
        .use_apll = false,                             // Use PLL clock
        .tx_desc_auto_clear = false,                   // RX only
        .fixed_mclk = 0,                               // No fixed MCLK
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,        // MCLK = 256 * Fs
        .bits_per_chan = I2S_BITS_PER_CHAN_32BIT,      // 32-bit channel width
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2sConfig, 0, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2S driver: %s", esp_err_to_name(err));
        return false;
    }

    // =========================================================================
    // CRITICAL: SPH0645 Timing Fixes (ESP32-S3 + SPH0645 alignment)
    // =========================================================================
    // SPH0645 requires:
    // 1. REG_SET_BIT(I2S_RX_TIMING_REG(I2S_PORT), BIT(9)); -> Delay sampling by 1 BCLK
    // 2. REG_SET_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT); -> Enable MSB shift
    //    (SPH0645 outputs MSB-first with 1 BCLK delay after WS, MSB shift aligns data)
    // =========================================================================
    
    // ESP32-S3 Specific Fixes for SPH0645
    REG_SET_BIT(I2S_RX_TIMING_REG(I2S_PORT), BIT(9));
    REG_SET_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_MSB_SHIFT);
    REG_SET_BIT(I2S_RX_CONF_REG(I2S_PORT), I2S_RX_WS_IDLE_POL);
    
    ESP_LOGI(TAG, "I2S driver installed (RIGHT channel, WS inverted, MSB shift, timing delay for SPH0645)");
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

    ESP_LOGI(TAG, "I2S pins: BCLK=%d WS=%d DIN=%d",
             I2S_BCLK_PIN, I2S_LRCL_PIN, I2S_DOUT_PIN);

    return true;
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
