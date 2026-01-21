/**
 * @file AudioCapture.cpp
 * @brief I2S audio capture implementation for SPH0645 MEMS microphone
 *
 * Uses ESP-IDF legacy I2S driver with ESP32-S3 register fixes for SPH0645.
 *
 * SPH0645 Sample Format:
 * - Outputs 18-bit data, MSB-first, in 32-bit I2S slots
 * - I2S configured for 32-bit samples, RIGHT slot on ESP32-S3 (SEL=GND wiring)
 * - Register fixes: MSB shift enabled, timing delay (BIT(9)), WS polarity inverted
 * - Conversion: >>14 shift with bias/clip, then scale to 16-bit
 * - DC removal handled in AudioNode
 *
 * @author LightwaveOS Team
 * @version 2.2.0
 */

#include "AudioCapture.h"

#if FEATURE_AUDIO_SYNC

#include <cstring>
#include <cmath>
#include <esp_timer.h>

// Unified logging system (preserves colored output conventions)
#define LW_LOG_TAG "AudioCapture"
#include "utils/Log.h"

// Runtime-configurable audio debug verbosity
#include "AudioDebugConfig.h"

// ESP32-S3 register access for WS polarity fix (SPH0645 requirement)
// The legacy I2S driver doesn't expose WS polarity, so we set the register directly
#include "soc/i2s_reg.h"

namespace lightwaveos {
namespace audio {

// Proven sample conversion constants
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
        LW_LOGW("Already initialized");
        return true;
    }

    LW_LOGI("Initializing I2S for SPH0645 (RIGHT channel)");

    // Configure I2S driver
    if (!configureI2S()) {
        LW_LOGE("Failed to configure I2S driver");
        return false;
    }

    // Set pin configuration
    if (!configurePins()) {
        LW_LOGE("Failed to configure I2S pins");
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    // Start I2S
    esp_err_t err = i2s_start(I2S_PORT);
    if (err != ESP_OK) {
        LW_LOGE("Failed to start I2S: %s", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    m_initialized = true;
    LW_LOGI("I2S initialized successfully");
    LW_LOGI("  Sample rate: %d Hz", SAMPLE_RATE);
    LW_LOGI("  Hop size: %d samples (%.1f ms)", HOP_SIZE, HOP_DURATION_MS);
    LW_LOGI("  Pins: BCLK=%d WS=%d DIN=%d", I2S_BCLK_PIN, I2S_LRCL_PIN, I2S_DOUT_PIN);
    LW_LOGI("  Channel: RIGHT slot (ESP32-S3 reads SEL=GND as RIGHT)");

    return true;
}

void AudioCapture::deinit()
{
    if (!m_initialized) {
        return;
    }

    LW_LOGI("Deinitializing I2S");

    // Stop I2S
    esp_err_t err = i2s_stop(I2S_PORT);
    if (err != ESP_OK) {
        LW_LOGW("Failed to stop I2S: %s", esp_err_to_name(err));
    }

    // Uninstall driver
    err = i2s_driver_uninstall(I2S_PORT);
    if (err != ESP_OK) {
        LW_LOGW("Failed to uninstall I2S driver: %s", esp_err_to_name(err));
    }

    m_initialized = false;
    LW_LOGI("I2S deinitialized");
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
        LW_LOGD("DMA timeout after %lu us", (unsigned long)readTimeUs);
        return CaptureResult::DMA_TIMEOUT;
    }

    if (err != ESP_OK) {
        m_stats.readErrors++;
        LW_LOGE("I2S read error: %s", esp_err_to_name(err));
        return CaptureResult::READ_ERROR;
    }

    // Verify we got the expected amount
    // DEFENSIVE CHECK: Validate samplesRead doesn't exceed HOP_SIZE before array access
    const size_t samplesRead = bytesRead / sizeof(int32_t);
    if (samplesRead < HOP_SIZE) {
        LW_LOGW("Partial read: %zu/%d samples", samplesRead, HOP_SIZE);
        // DEFENSIVE CHECK: Ensure samplesRead is within bounds before memset
        if (samplesRead < HOP_SIZE) {
            memset(&buffer[samplesRead], 0, (HOP_SIZE - samplesRead) * sizeof(int16_t));
        }
    } else if (samplesRead > HOP_SIZE) {
        // DEFENSIVE CHECK: Clamp samplesRead to HOP_SIZE if corrupted
        LW_LOGW("Oversized read: %zu/%d samples, clamping", samplesRead, HOP_SIZE);
        // Process only HOP_SIZE samples
    }

    // DMA debug log - gated by verbosity >= 3
    // Use time-based rate limiting (2 seconds minimum) to prevent serial spam
    auto& dbgCfgDMA = getAudioDebugConfig();
    static uint32_t s_lastDmaLogMs = 0;
    static bool s_firstPrint = true;
    uint32_t nowMs = millis();
    if (dbgCfgDMA.verbosity >= 3 && (s_firstPrint || (nowMs - s_lastDmaLogMs >= 2000))) {
        s_firstPrint = false;
        s_lastDmaLogMs = nowMs;
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

        // Title-only coloring: color resets before values for readability at 62.5Hz
        LW_LOGI(LW_CLR_YELLOW "DMA dbg:" LW_ANSI_RESET " ch=%s msb_shift=%s raw0=%08X raw1=%08X min=%ld max=%ld "
                 "pk>>8=%ld pk>>10=%ld pk>>12=%ld pk>>14=%ld pk>>16=%ld",
                 channelFmt, msbShiftEnabled ? "ON" : "OFF",
                 (uint32_t)m_dmaBuffer[0], (uint32_t)m_dmaBuffer[1],
                 (long)rawMin, (long)rawMax,
                 (long)peakShift(8), (long)peakShift(10), (long)peakShift(12),
                 (long)peakShift(14), (long)peakShift(16));
    }

    // Convert samples from 32-bit I2S slots to 16-bit signed samples
    // Emotiscope 2.0 exact conversion pipeline:
    // 1. >> 14 shift to extract 18-bit value
    // 2. Add 7000 offset
    // 3. Clip to ±131072 (18-bit range)
    // 4. Subtract 360 offset
    // 5. Scale to float [-1.0, 1.0] using recip_scale = 1.0 / 131072.0
    // 6. Apply 4x amplification (Emotiscope fixed gain)
    // 7. Convert back to int16 by multiplying by 32768.0 and rounding
    
    static constexpr float RECIP_SCALE = 1.0f / 131072.0f;  // Max 18-bit signed value
    static constexpr float FIXED_GAIN = 4.0f;                // Emotiscope fixed amplification
    static constexpr int32_t OFFSET_1 = 7000;
    static constexpr int32_t OFFSET_2 = 360;
    static constexpr int32_t CLIP_MIN_18 = -131072;
    static constexpr int32_t CLIP_MAX_18 = 131072;
    
    int16_t peak = 0;
    // Process 4 samples at a time (vectorized for efficiency)
    size_t vectorizedEnd = (HOP_SIZE / 4) * 4;
    for (size_t i = 0; i < vectorizedEnd; i+=4) {
        // Emotiscope exact conversion (vectorized 4 samples for efficiency)
        int32_t raw0 = m_dmaBuffer[i+0];
        int32_t raw1 = m_dmaBuffer[i+1];
        int32_t raw2 = m_dmaBuffer[i+2];
        int32_t raw3 = m_dmaBuffer[i+3];
        
        // >> 14 shift, add 7000, clip, subtract 360 (matching Emotiscope microphone.h:104-107)
        int32_t s0 = (raw0 >> 14) + OFFSET_1;
        int32_t s1 = (raw1 >> 14) + OFFSET_1;
        int32_t s2 = (raw2 >> 14) + OFFSET_1;
        int32_t s3 = (raw3 >> 14) + OFFSET_1;
        
        // Clip to 18-bit range
        if (s0 < CLIP_MIN_18) s0 = CLIP_MIN_18;
        if (s0 > CLIP_MAX_18) s0 = CLIP_MAX_18;
        if (s1 < CLIP_MIN_18) s1 = CLIP_MIN_18;
        if (s1 > CLIP_MAX_18) s1 = CLIP_MAX_18;
        if (s2 < CLIP_MIN_18) s2 = CLIP_MIN_18;
        if (s2 > CLIP_MAX_18) s2 = CLIP_MAX_18;
        if (s3 < CLIP_MIN_18) s3 = CLIP_MIN_18;
        if (s3 > CLIP_MAX_18) s3 = CLIP_MAX_18;
        
        // Subtract 360 offset
        s0 -= OFFSET_2;
        s1 -= OFFSET_2;
        s2 -= OFFSET_2;
        s3 -= OFFSET_2;
        
        // Convert to float [-1.0, 1.0] range (matching Emotiscope microphone.h:111)
        float f0 = (float)s0 * RECIP_SCALE;
        float f1 = (float)s1 * RECIP_SCALE;
        float f2 = (float)s2 * RECIP_SCALE;
        float f3 = (float)s3 * RECIP_SCALE;
        
        // Apply 4x amplification (Emotiscope microphone.h:114)
        f0 *= FIXED_GAIN;
        f1 *= FIXED_GAIN;
        f2 *= FIXED_GAIN;
        f3 *= FIXED_GAIN;
        
        // Convert back to int16 (multiply by 32768.0 and round)
        int32_t i0 = (int32_t)lroundf(f0 * 32768.0f);
        int32_t i1 = (int32_t)lroundf(f1 * 32768.0f);
        int32_t i2 = (int32_t)lroundf(f2 * 32768.0f);
        int32_t i3 = (int32_t)lroundf(f3 * 32768.0f);
        
        // Clip to int16 range
        if (i0 < -32768) i0 = -32768;
        if (i0 > 32767) i0 = 32767;
        if (i1 < -32768) i1 = -32768;
        if (i1 > 32767) i1 = 32767;
        if (i2 < -32768) i2 = -32768;
        if (i2 > 32767) i2 = 32767;
        if (i3 < -32768) i3 = -32768;
        if (i3 > 32767) i3 = 32767;
        
        buffer[i+0] = (int16_t)i0;
        buffer[i+1] = (int16_t)i1;
        buffer[i+2] = (int16_t)i2;
        buffer[i+3] = (int16_t)i3;
        
        // Track peak for level metering
        int16_t abs0 = (i0 < 0) ? -i0 : i0;
        int16_t abs1 = (i1 < 0) ? -i1 : i1;
        int16_t abs2 = (i2 < 0) ? -i2 : i2;
        int16_t abs3 = (i3 < 0) ? -i3 : i3;
        if (abs0 > peak) peak = abs0;
        if (abs1 > peak) peak = abs1;
        if (abs2 > peak) peak = abs2;
        if (abs3 > peak) peak = abs3;
    }
    
    // Handle remaining samples if HOP_SIZE is not divisible by 4
    for (size_t i = vectorizedEnd; i < HOP_SIZE; ++i) {
        int32_t raw = m_dmaBuffer[i];
        
        // >> 14 shift, add 7000, clip, subtract 360
        int32_t s = (raw >> 14) + OFFSET_1;
        if (s < CLIP_MIN_18) s = CLIP_MIN_18;
        if (s > CLIP_MAX_18) s = CLIP_MAX_18;
        s -= OFFSET_2;
        
        // Convert to float [-1.0, 1.0] range
        float f = (float)s * RECIP_SCALE;
        
        // Apply 4x amplification
        f *= FIXED_GAIN;
        
        // Convert back to int16
        int32_t sample = (int32_t)lroundf(f * 32768.0f);
        if (sample < -32768) sample = -32768;
        if (sample > 32767) sample = 32767;
        
        buffer[i] = (int16_t)sample;
        
        // Track peak
        int16_t absSample = (sample < 0) ? -sample : sample;
        if (absSample > peak) peak = absSample;
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
    // ESP32-S3 quirk: SPH0645 SEL=GND outputs LEFT per I2S spec, but ESP32-S3 reads it as RIGHT
    i2s_config_t i2sConfig = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,  // 32-bit slots for SPH0645
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,   // ESP32-S3: RIGHT slot for SEL=GND mic
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
        LW_LOGE("Failed to install I2S driver: %s", esp_err_to_name(err));
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

    LW_LOGI("I2S driver installed (RIGHT slot, WS inverted, MSB shift, timing delay for SPH0645)");
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
        LW_LOGE("Failed to set I2S pins: %s", esp_err_to_name(err));
        return false;
    }

    LW_LOGI("I2S pins: BCLK=%d WS=%d DIN=%d",
             I2S_BCLK_PIN, I2S_LRCL_PIN, I2S_DOUT_PIN);

    return true;
}

} // namespace audio
} // namespace lightwaveos

#endif // FEATURE_AUDIO_SYNC
