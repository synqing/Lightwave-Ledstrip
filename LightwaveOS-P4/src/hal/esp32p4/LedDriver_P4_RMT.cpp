/**
 * @file LedDriver_P4_RMT.cpp
 * @brief Custom parallel RMT driver implementation for ESP32-P4
 *
 * Implements Emotiscope-style parallel RMT transmission with temporal dithering.
 * Based on analysis of Emotiscope 2.0 by @lixielabs (GPLv3)
 */

#include "LedDriver_P4_RMT.h"

#include <cstring>
#include <cmath>

#ifndef NATIVE_BUILD
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_random.h"
#endif

#define LW_LOG_TAG "LedDriver_P4_RMT"
#include "utils/Log.h"

namespace lightwaveos {
namespace hal {

// ============================================================================
// RMT Encoder Implementation (Emotiscope-style)
// ============================================================================

#ifndef NATIVE_BUILD

/**
 * @brief Custom encoder function for WS2812 LED strips
 *
 * State machine:
 * - State 0: Encode RGB byte data
 * - State 1: Append reset pulse
 */
IRAM_ATTR static size_t rmt_encode_led_strip(
    rmt_encoder_t* encoder,
    rmt_channel_handle_t channel,
    const void* primary_data,
    size_t data_size,
    rmt_encode_state_t* ret_state
) {
    LedStripEncoder* led_encoder = __containerof(encoder, LedStripEncoder, base);
    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    switch (led_encoder->state) {
        case 0:  // Encode RGB byte data
            encoded_symbols += bytes_encoder->encode(
                bytes_encoder,
                channel,
                primary_data,
                data_size,
                &session_state
            );

            if (session_state & RMT_ENCODING_COMPLETE) {
                led_encoder->state = 1;  // Move to reset state
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = static_cast<rmt_encode_state_t>(
                    state | static_cast<uint32_t>(RMT_ENCODING_MEM_FULL)
                );
                goto out;  // Yield if memory full
            }
            // Fall through to state 1

        case 1:  // Append reset pulse
            encoded_symbols += copy_encoder->encode(
                copy_encoder,
                channel,
                &led_encoder->reset_code,
                sizeof(led_encoder->reset_code),
                &session_state
            );

            if (session_state & RMT_ENCODING_COMPLETE) {
                led_encoder->state = RMT_ENCODING_RESET;  // Back to initial state
                state = static_cast<rmt_encode_state_t>(
                    state | static_cast<uint32_t>(RMT_ENCODING_COMPLETE)
                );
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = static_cast<rmt_encode_state_t>(
                    state | static_cast<uint32_t>(RMT_ENCODING_MEM_FULL)
                );
                goto out;
            }
            break;
    }

out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t* encoder) {
    LedStripEncoder* led_encoder = __containerof(encoder, LedStripEncoder, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    return ESP_OK;
}

static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t* encoder) {
    LedStripEncoder* led_encoder = __containerof(encoder, LedStripEncoder, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

#endif  // NATIVE_BUILD

// ============================================================================
// Constructor / Destructor
// ============================================================================

LedDriver_P4_RMT::LedDriver_P4_RMT() {
    resetStats();
    memset(m_strip1, 0, sizeof(m_strip1));
    memset(m_strip2, 0, sizeof(m_strip2));
    memset(m_rawBuffer, 0, sizeof(m_rawBuffer));
    memset(m_ditherError, 0, sizeof(m_ditherError));

#ifndef NATIVE_BUILD
    memset(&m_stripEncoderA, 0, sizeof(m_stripEncoderA));
    memset(&m_stripEncoderB, 0, sizeof(m_stripEncoderB));
    memset(&m_txConfig, 0, sizeof(m_txConfig));
#endif
}

LedDriver_P4_RMT::~LedDriver_P4_RMT() {
    deinit();
}

// ============================================================================
// Initialization
// ============================================================================

bool LedDriver_P4_RMT::init(const LedStripConfig& config) {
    m_dual = false;
    m_config1 = config;
    m_stripCounts[0] = config.ledCount;
    m_stripCounts[1] = 0;
    m_totalLeds = config.ledCount;
    m_brightness = config.brightness;

    if (config.ledCount > kMaxLedsPerStrip) {
        LW_LOGE("LED count exceeds max: %u > %u", config.ledCount, kMaxLedsPerStrip);
        return false;
    }

#ifndef NATIVE_BUILD
    // Initialize RMT channel A
    if (!initRmtChannel(chip::gpio::LED_STRIP1_DATA, &m_txChanA)) {
        LW_LOGE("Failed to init RMT channel A");
        return false;
    }

    // Create encoders
    if (!createEncoders()) {
        LW_LOGE("Failed to create RMT encoders");
        rmt_del_channel(m_txChanA);
        return false;
    }

    // Enable channel
    ESP_ERROR_CHECK(rmt_enable(m_txChanA));

    // Configure transmit settings
    m_txConfig.loop_count = 0;
    m_txConfig.flags.eot_level = 0;
    m_txConfig.flags.queue_nonblocking = 0;
#endif

    // Initialize random dither error
    initRandomDitherError();

    m_initialized = true;
    m_firstFrame = true;

    LW_LOGI("RMT driver init: %u LEDs on GPIO %u (single strip)",
            config.ledCount, chip::gpio::LED_STRIP1_DATA);
    return true;
}

bool LedDriver_P4_RMT::initDual(const LedStripConfig& config1,
                                 const LedStripConfig& config2) {
    m_dual = true;
    m_config1 = config1;
    m_config2 = config2;
    m_stripCounts[0] = config1.ledCount;
    m_stripCounts[1] = config2.ledCount;
    m_totalLeds = config1.ledCount + config2.ledCount;
    m_brightness = config1.brightness;

    if (config1.ledCount > kMaxLedsPerStrip ||
        config2.ledCount > kMaxLedsPerStrip) {
        LW_LOGE("LED count exceeds max: %u/%u > %u",
                config1.ledCount, config2.ledCount, kMaxLedsPerStrip);
        return false;
    }

#ifndef NATIVE_BUILD
    // Initialize RMT channel A (strip 1)
    if (!initRmtChannel(chip::gpio::LED_STRIP1_DATA, &m_txChanA)) {
        LW_LOGE("Failed to init RMT channel A");
        return false;
    }

    // Initialize RMT channel B (strip 2)
    if (!initRmtChannel(chip::gpio::LED_STRIP2_DATA, &m_txChanB)) {
        LW_LOGE("Failed to init RMT channel B");
        rmt_del_channel(m_txChanA);
        return false;
    }

    // Create encoders
    if (!createEncoders()) {
        LW_LOGE("Failed to create RMT encoders");
        rmt_del_channel(m_txChanA);
        rmt_del_channel(m_txChanB);
        return false;
    }

    // Enable both channels
    ESP_ERROR_CHECK(rmt_enable(m_txChanA));
    ESP_ERROR_CHECK(rmt_enable(m_txChanB));

    // Configure transmit settings
    m_txConfig.loop_count = 0;
    m_txConfig.flags.eot_level = 0;
    m_txConfig.flags.queue_nonblocking = 0;
#endif

    // Initialize random dither error
    initRandomDitherError();

    m_initialized = true;
    m_firstFrame = true;

    LW_LOGI("RMT driver init: 2x%u LEDs on GPIO %u/%u (dual strip, parallel)",
            config1.ledCount,
            chip::gpio::LED_STRIP1_DATA,
            chip::gpio::LED_STRIP2_DATA);
    return true;
}

bool LedDriver_P4_RMT::initRmtChannel(uint8_t gpio,
                                       rmt_channel_handle_t* channel) {
#ifndef NATIVE_BUILD
    rmt_tx_channel_config_t tx_config = {
        .gpio_num = static_cast<gpio_num_t>(gpio),
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = kRmtResolutionHz,
        .mem_block_symbols = kRmtMemBlockSymbols,
        .trans_queue_depth = kRmtTransQueueDepth,
        .intr_priority = 0,
        .flags = {
            .invert_out = 0,
            .with_dma = 0,  // No DMA (Emotiscope-style)
            .io_loop_back = 0,
            .io_od_mode = 0
        }
    };

    esp_err_t ret = rmt_new_tx_channel(&tx_config, channel);
    if (ret != ESP_OK) {
        LW_LOGE("rmt_new_tx_channel failed for GPIO %u: %s",
                gpio, esp_err_to_name(ret));
        return false;
    }

    return true;
#else
    (void)gpio;
    (void)channel;
    return true;
#endif
}

bool LedDriver_P4_RMT::createEncoders() {
#ifndef NATIVE_BUILD
    // WS2812 byte encoder configuration
    // Timing: bit0 = {T0H, 1, T0L, 0}, bit1 = {T1H, 1, T1L, 0}
    rmt_bytes_encoder_config_t bytes_config = {
        .bit0 = {{kT0H, 1, kT0L, 0}},
        .bit1 = {{kT1H, 1, kT1L, 0}},
        .flags = {.msb_first = 1}
    };

    rmt_copy_encoder_config_t copy_config = {};

    // Encoder A
    m_stripEncoderA.base.encode = rmt_encode_led_strip;
    m_stripEncoderA.base.del = rmt_del_led_strip_encoder;
    m_stripEncoderA.base.reset = rmt_led_strip_encoder_reset;
    m_stripEncoderA.state = 0;
    m_stripEncoderA.reset_code = (rmt_symbol_word_t){{kResetTicks, 0, kResetTicks, 0}};

    esp_err_t ret = rmt_new_bytes_encoder(&bytes_config, &m_stripEncoderA.bytes_encoder);
    if (ret != ESP_OK) {
        LW_LOGE("Failed to create bytes encoder A: %s", esp_err_to_name(ret));
        return false;
    }

    ret = rmt_new_copy_encoder(&copy_config, &m_stripEncoderA.copy_encoder);
    if (ret != ESP_OK) {
        rmt_del_encoder(m_stripEncoderA.bytes_encoder);
        LW_LOGE("Failed to create copy encoder A: %s", esp_err_to_name(ret));
        return false;
    }

    m_encoderA = &m_stripEncoderA.base;

    if (m_dual) {
        // Encoder B
        m_stripEncoderB.base.encode = rmt_encode_led_strip;
        m_stripEncoderB.base.del = rmt_del_led_strip_encoder;
        m_stripEncoderB.base.reset = rmt_led_strip_encoder_reset;
        m_stripEncoderB.state = 0;
        m_stripEncoderB.reset_code = (rmt_symbol_word_t){{kResetTicks, 0, kResetTicks, 0}};

        ret = rmt_new_bytes_encoder(&bytes_config, &m_stripEncoderB.bytes_encoder);
        if (ret != ESP_OK) {
            rmt_del_encoder(m_stripEncoderA.bytes_encoder);
            rmt_del_encoder(m_stripEncoderA.copy_encoder);
            LW_LOGE("Failed to create bytes encoder B: %s", esp_err_to_name(ret));
            return false;
        }

        ret = rmt_new_copy_encoder(&copy_config, &m_stripEncoderB.copy_encoder);
        if (ret != ESP_OK) {
            rmt_del_encoder(m_stripEncoderA.bytes_encoder);
            rmt_del_encoder(m_stripEncoderA.copy_encoder);
            rmt_del_encoder(m_stripEncoderB.bytes_encoder);
            LW_LOGE("Failed to create copy encoder B: %s", esp_err_to_name(ret));
            return false;
        }

        m_encoderB = &m_stripEncoderB.base;
    }

    return true;
#else
    return true;
#endif
}

void LedDriver_P4_RMT::deinit() {
    if (!m_initialized) return;

#ifndef NATIVE_BUILD
    // Disable and delete channels
    if (m_txChanA) {
        rmt_disable(m_txChanA);
        rmt_del_channel(m_txChanA);
        m_txChanA = nullptr;
    }

    if (m_txChanB) {
        rmt_disable(m_txChanB);
        rmt_del_channel(m_txChanB);
        m_txChanB = nullptr;
    }

    // Delete encoders (bytes and copy encoders deleted via del callback)
    if (m_encoderA) {
        rmt_del_encoder(m_encoderA);
        m_encoderA = nullptr;
    }

    if (m_encoderB) {
        rmt_del_encoder(m_encoderB);
        m_encoderB = nullptr;
    }
#endif

    m_initialized = false;
}

// ============================================================================
// Buffer Access
// ============================================================================

CRGB* LedDriver_P4_RMT::getBuffer() {
    return m_strip1;
}

CRGB* LedDriver_P4_RMT::getBuffer(uint8_t stripIndex) {
    if (stripIndex == 0) return m_strip1;
    if (stripIndex == 1) return m_strip2;
    return nullptr;
}

uint16_t LedDriver_P4_RMT::getTotalLedCount() const {
    return m_totalLeds;
}

uint16_t LedDriver_P4_RMT::getLedCount(uint8_t stripIndex) const {
    return (stripIndex < 2) ? m_stripCounts[stripIndex] : 0;
}

// ============================================================================
// Show (Main Transmission Function)
// ============================================================================

void LedDriver_P4_RMT::show() {
#ifndef NATIVE_BUILD
    uint32_t start = static_cast<uint32_t>(esp_timer_get_time());

    // ========================================================================
    // DOUBLE-BUFFERING: Wait for PREVIOUS frame to complete (not current!)
    // This allows CPU to work on next frame while RMT transmits current frame
    // ========================================================================
    if (!m_firstFrame) {
        rmt_tx_wait_all_done(m_txChanA, portMAX_DELAY);
        if (m_dual && m_txChanB) {
            rmt_tx_wait_all_done(m_txChanB, portMAX_DELAY);
        }
    }
    m_firstFrame = false;

    uint32_t t1 = static_cast<uint32_t>(esp_timer_get_time());

    // ========================================================================
    // Quantize CRGB to uint8 with temporal dithering
    // ========================================================================
    uint8_t* rawStrip1 = m_rawBuffer;
    uint8_t* rawStrip2 = m_rawBuffer + (m_stripCounts[0] * kBytesPerPixel);

    if (m_ditheringEnabled) {
        quantizeWithDithering(m_strip1, rawStrip1, &m_ditherError[0], m_stripCounts[0]);
        if (m_dual) {
            quantizeWithDithering(m_strip2, rawStrip2,
                                  &m_ditherError[kMaxLedsPerStrip], m_stripCounts[1]);
        }
    } else {
        quantizeSimple(m_strip1, rawStrip1, m_stripCounts[0]);
        if (m_dual) {
            quantizeSimple(m_strip2, rawStrip2, m_stripCounts[1]);
        }
    }

    uint32_t t2 = static_cast<uint32_t>(esp_timer_get_time());

    // ========================================================================
    // PARALLEL TRANSMISSION: Start both strips simultaneously
    // ========================================================================
    esp_err_t ret1 = rmt_transmit(m_txChanA, m_encoderA, rawStrip1,
                                   m_stripCounts[0] * kBytesPerPixel, &m_txConfig);
    if (ret1 != ESP_OK) {
        LW_LOGE("RMT transmit A failed: %s", esp_err_to_name(ret1));
    }

    if (m_dual && m_txChanB && m_encoderB) {
        esp_err_t ret2 = rmt_transmit(m_txChanB, m_encoderB, rawStrip2,
                                       m_stripCounts[1] * kBytesPerPixel, &m_txConfig);
        if (ret2 != ESP_OK) {
            LW_LOGE("RMT transmit B failed: %s", esp_err_to_name(ret2));
        }
    }

    uint32_t end = static_cast<uint32_t>(esp_timer_get_time());

    // Update statistics
    updateShowStats(end - start);

    // Detailed timing log every ~2 seconds at 120 FPS
    if (m_stats.frameCount % 240 == 0) {
        LW_LOGI("Show timing: wait=%uus, quantize=%uus, transmit_start=%uus, total=%uus",
                t1 - start, t2 - t1, end - t2, end - start);
    }

#else
    m_stats.frameCount++;
#endif
}

// ============================================================================
// Quantization with Temporal Dithering (Emotiscope-style)
// ============================================================================

void LedDriver_P4_RMT::quantizeWithDithering(const CRGB* src, uint8_t* dst,
                                              DitherError* ditherError,
                                              uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
        // Apply brightness scaling
        uint16_t r = (static_cast<uint16_t>(src[i].r) * m_brightness) >> 8;
        uint16_t g = (static_cast<uint16_t>(src[i].g) * m_brightness) >> 8;
        uint16_t b = (static_cast<uint16_t>(src[i].b) * m_brightness) >> 8;

        // Convert to float for dithering (Emotiscope uses 0.0-1.0 range)
        float rF = static_cast<float>(r);
        float gF = static_cast<float>(g);
        float bF = static_cast<float>(b);

        // Quantize to integer
        uint8_t rOut = static_cast<uint8_t>(rF);
        uint8_t gOut = static_cast<uint8_t>(gF);
        uint8_t bOut = static_cast<uint8_t>(bF);

        // Calculate quantization error
        float errorR = rF - rOut;
        float errorG = gF - gOut;
        float errorB = bF - bOut;

        // Accumulate error above threshold (Emotiscope threshold: 0.0275)
        // Scale threshold for 0-255 range: 0.0275 * 255 ≈ 7
        constexpr float kDitherThresholdScaled = 7.0f;

        if (errorR >= kDitherThresholdScaled * 0.5f) {
            ditherError[i].r += errorR;
        }
        if (errorG >= kDitherThresholdScaled * 0.5f) {
            ditherError[i].g += errorG;
        }
        if (errorB >= kDitherThresholdScaled * 0.5f) {
            ditherError[i].b += errorB;
        }

        // When accumulated error >= 1.0, add 1 to output and subtract from accumulator
        if (ditherError[i].r >= 1.0f) {
            if (rOut < 255) rOut += 1;
            ditherError[i].r -= 1.0f;
        }
        if (ditherError[i].g >= 1.0f) {
            if (gOut < 255) gOut += 1;
            ditherError[i].g -= 1.0f;
        }
        if (ditherError[i].b >= 1.0f) {
            if (bOut < 255) bOut += 1;
            ditherError[i].b -= 1.0f;
        }

        // Output in GRB order (WS2812 format)
        dst[i * 3 + 0] = gOut;
        dst[i * 3 + 1] = rOut;
        dst[i * 3 + 2] = bOut;
    }
}

void LedDriver_P4_RMT::quantizeSimple(const CRGB* src, uint8_t* dst,
                                       uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
        // Apply brightness scaling
        uint8_t r = (static_cast<uint16_t>(src[i].r) * m_brightness) >> 8;
        uint8_t g = (static_cast<uint16_t>(src[i].g) * m_brightness) >> 8;
        uint8_t b = (static_cast<uint16_t>(src[i].b) * m_brightness) >> 8;

        // Output in GRB order (WS2812 format)
        dst[i * 3 + 0] = g;
        dst[i * 3 + 1] = r;
        dst[i * 3 + 2] = b;
    }
}

// ============================================================================
// Brightness / Power
// ============================================================================

void LedDriver_P4_RMT::setBrightness(uint8_t brightness) {
    m_brightness = brightness;
    m_stats.currentBrightness = brightness;
}

uint8_t LedDriver_P4_RMT::getBrightness() const {
    return m_brightness;
}

void LedDriver_P4_RMT::setMaxPower(uint8_t volts, uint16_t milliamps) {
    (void)volts;  // Always 5V for WS2812
    m_maxMilliamps = milliamps;
    // TODO: Implement power limiting (calculate total power, scale brightness)
}

// ============================================================================
// Buffer Operations
// ============================================================================

void LedDriver_P4_RMT::clear(bool showNow) {
    fill(CRGB::Black, showNow);
}

void LedDriver_P4_RMT::fill(CRGB color, bool showNow) {
    for (uint16_t i = 0; i < m_stripCounts[0]; i++) {
        m_strip1[i] = color;
    }
    for (uint16_t i = 0; i < m_stripCounts[1]; i++) {
        m_strip2[i] = color;
    }
    if (showNow) {
        show();
    }
}

void LedDriver_P4_RMT::setPixel(uint16_t index, CRGB color) {
    if (index < m_stripCounts[0]) {
        m_strip1[index] = color;
        return;
    }
    uint16_t strip2Index = index - m_stripCounts[0];
    if (strip2Index < m_stripCounts[1]) {
        m_strip2[strip2Index] = color;
    }
}

// ============================================================================
// Statistics
// ============================================================================

void LedDriver_P4_RMT::resetStats() {
    m_stats = LedDriverStats{};
}

void LedDriver_P4_RMT::updateShowStats(uint32_t showUs) {
    m_stats.frameCount++;
    m_stats.lastShowUs = showUs;

    if (showUs > m_stats.maxShowUs) {
        m_stats.maxShowUs = showUs;
    }

    if (m_stats.frameCount == 1) {
        m_stats.avgShowUs = showUs;
    } else {
        // Exponential moving average (7/8 old + 1/8 new)
        m_stats.avgShowUs = (m_stats.avgShowUs * 7 + showUs) / 8;
    }
}

void LedDriver_P4_RMT::initRandomDitherError() {
#ifndef NATIVE_BUILD
    // Initialize with random values to prevent startup banding
    for (uint16_t i = 0; i < kMaxLedsPerStrip * 2; i++) {
        m_ditherError[i].r = static_cast<float>(esp_random() % 256) / 255.0f;
        m_ditherError[i].g = static_cast<float>(esp_random() % 256) / 255.0f;
        m_ditherError[i].b = static_cast<float>(esp_random() % 256) / 255.0f;
    }
#endif
}

} // namespace hal
} // namespace lightwaveos
