// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file LedDriver_P4_RMT.h
 * @brief Custom parallel RMT driver for ESP32-P4 WS2812 LED strips
 *
 * This driver implements high-performance parallel transmission for dual
 * WS2812 LED strips using ESP-IDF's RMT peripheral directly (no FastLED).
 *
 * Key features:
 * - Parallel transmission on 2 RMT channels (strips transmit simultaneously)
 * - Double-buffering (wait for previous frame at start, not end)
 * - Temporal dithering for perceived 10-12 bit color depth
 * - Custom RMT encoder (Emotiscope-style, no DMA)
 * - Maintains CRGB interface for effect compatibility
 *
 * Performance target: <5ms show() time for 320 LEDs @ 120 FPS
 *
 * Based on analysis of Emotiscope 2.0 by @lixielabs (GPLv3)
 */

#pragma once

#if !defined(CONFIG_IDF_TARGET_ESP32P4) && !defined(CHIP_ESP32_P4)
#error "LedDriver_P4_RMT is ESP32-P4 only. Exclude hal/esp32p4 from non-P4 builds."
#endif

#include "hal/interface/ILedDriver.h"
#include "config/chip_config.h"

#ifndef NATIVE_BUILD
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#endif

namespace lightwaveos {
namespace hal {

/**
 * @brief Dither error accumulator for temporal dithering
 */
struct DitherError {
    float r;
    float g;
    float b;
};

/**
 * @brief Custom RMT encoder for WS2812 LED strips
 */
struct LedStripEncoder {
#ifndef NATIVE_BUILD
    rmt_encoder_t base;
    rmt_encoder_t* bytes_encoder;
    rmt_encoder_t* copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
#endif
};

/**
 * @brief High-performance parallel RMT LED driver for ESP32-P4
 *
 * Implements ILedDriver interface with custom RMT backend optimized for
 * dual-strip parallel transmission. Compatible with existing effect code
 * that uses CRGB buffers.
 */
class LedDriver_P4_RMT : public ILedDriver {
public:
    LedDriver_P4_RMT();
    ~LedDriver_P4_RMT() override;

    // ========================================================================
    // ILedDriver Interface Implementation
    // ========================================================================

    bool init(const LedStripConfig& config) override;
    bool initDual(const LedStripConfig& config1, const LedStripConfig& config2) override;
    void deinit() override;

    CRGB* getBuffer() override;
    CRGB* getBuffer(uint8_t stripIndex) override;

    uint16_t getTotalLedCount() const override;
    uint16_t getLedCount(uint8_t stripIndex) const override;

    void show() override;
    void setBrightness(uint8_t brightness) override;
    uint8_t getBrightness() const override;
    void setMaxPower(uint8_t volts, uint16_t milliamps) override;

    void clear(bool show = false) override;
    void fill(CRGB color, bool show = false) override;
    void setPixel(uint16_t index, CRGB color) override;

    bool isInitialized() const override { return m_initialized; }
    const LedDriverStats& getStats() const override { return m_stats; }
    void resetStats() override;

    // ========================================================================
    // Driver-Specific Configuration
    // ========================================================================

    /**
     * @brief Enable/disable temporal dithering
     * @param enable True to enable dithering (default: enabled)
     */
    void setDitheringEnabled(bool enable) { m_ditheringEnabled = enable; }

    /**
     * @brief Check if dithering is enabled
     */
    bool isDitheringEnabled() const { return m_ditheringEnabled; }

private:
    // ========================================================================
    // Constants
    // ========================================================================

    static constexpr uint16_t kMaxLedsPerStrip = 160;
    static constexpr uint8_t kBytesPerPixel = 3;  // GRB for WS2812
    static constexpr uint32_t kRmtResolutionHz = 10000000;  // 10 MHz (0.1µs tick)
    static constexpr uint16_t kRmtMemBlockSymbols = 128;    // Emotiscope-style
    static constexpr uint8_t kRmtTransQueueDepth = 4;

    // WS2812 timing in RMT ticks (at 10 MHz, 1 tick = 0.1µs)
    // T0H: 0.4µs = 4 ticks, T0L: 0.85µs = 8.5 ticks → use 4,6
    // T1H: 0.8µs = 8 ticks, T1L: 0.45µs = 4.5 ticks → use 7,6
    static constexpr uint16_t kT0H = 4;   // Logic 0: high time
    static constexpr uint16_t kT0L = 6;   // Logic 0: low time
    static constexpr uint16_t kT1H = 7;   // Logic 1: high time
    static constexpr uint16_t kT1L = 6;   // Logic 1: low time
    static constexpr uint16_t kResetTicks = 250;  // Reset: 25µs low (>50µs total)

    // Dithering threshold (Emotiscope uses 0.0275)
    static constexpr float kDitherThreshold = 0.0275f;

    // ========================================================================
    // Configuration State
    // ========================================================================

    LedStripConfig m_config1{};
    LedStripConfig m_config2{};
    uint16_t m_stripCounts[2] = {0, 0};
    uint16_t m_totalLeds = 0;
    uint8_t m_brightness = 128;
    uint16_t m_maxMilliamps = 3000;
    bool m_initialized = false;
    bool m_dual = false;
    bool m_ditheringEnabled = true;
    bool m_firstFrame = true;

    // ========================================================================
    // LED Buffers
    // ========================================================================

    // CRGB buffers (effects write here via getBuffer())
    CRGB m_strip1[kMaxLedsPerStrip];
    CRGB m_strip2[kMaxLedsPerStrip];

    // 8-bit raw output buffer (GRB order, sent to RMT)
    uint8_t m_rawBuffer[kMaxLedsPerStrip * 2 * kBytesPerPixel];

    // Temporal dithering error accumulators
    DitherError m_ditherError[kMaxLedsPerStrip * 2];

    // ========================================================================
    // RMT Resources
    // ========================================================================

#ifndef NATIVE_BUILD
    rmt_channel_handle_t m_txChanA = nullptr;
    rmt_channel_handle_t m_txChanB = nullptr;
    rmt_encoder_handle_t m_encoderA = nullptr;
    rmt_encoder_handle_t m_encoderB = nullptr;

    LedStripEncoder m_stripEncoderA;
    LedStripEncoder m_stripEncoderB;

    rmt_transmit_config_t m_txConfig;
#endif

    // ========================================================================
    // Statistics
    // ========================================================================

    LedDriverStats m_stats{};

    // ========================================================================
    // Private Methods
    // ========================================================================

    /**
     * @brief Initialize RMT TX channel
     */
    bool initRmtChannel(uint8_t gpio, rmt_channel_handle_t* channel);

    /**
     * @brief Create custom LED strip encoder (shared between channels)
     */
    bool createEncoders();

    /**
     * @brief Quantize CRGB to uint8 with temporal dithering
     * @param src Source CRGB buffer
     * @param dst Destination uint8 buffer (GRB order)
     * @param ditherError Dither error accumulator for this strip
     * @param count Number of LEDs
     */
    void quantizeWithDithering(const CRGB* src, uint8_t* dst,
                               DitherError* ditherError, uint16_t count);

    /**
     * @brief Quantize CRGB to uint8 without dithering
     */
    void quantizeSimple(const CRGB* src, uint8_t* dst, uint16_t count);

    /**
     * @brief Apply brightness scaling to a pixel
     */
    inline void applyBrightness(uint8_t& r, uint8_t& g, uint8_t& b) const;

    /**
     * @brief Update performance statistics
     */
    void updateShowStats(uint32_t showUs);

    /**
     * @brief Initialize random dither error (prevents startup banding)
     */
    void initRandomDitherError();
};

} // namespace hal
} // namespace lightwaveos
