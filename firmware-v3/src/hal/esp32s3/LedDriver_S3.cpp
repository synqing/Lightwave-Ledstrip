#include "LedDriver_S3.h"

#include <cstring>

#ifndef NATIVE_BUILD
#include <FastLED.h>
#include <esp_timer.h>
#endif

#define LW_LOG_TAG "LedDriver_S3"
#include "utils/Log.h"

namespace lightwaveos {
namespace hal {

LedDriver_S3::LedDriver_S3() {
    resetStats();
    memset(m_strip1, 0, sizeof(m_strip1));
    memset(m_strip2, 0, sizeof(m_strip2));
}

bool LedDriver_S3::init(const LedStripConfig& config) {
    m_dual = false;
    m_config1 = config;
    m_stripCounts[0] = config.ledCount;
    m_stripCounts[1] = 0;
    m_totalLeds = config.ledCount;

    if (config.ledCount > kMaxLedsPerStrip) {
        LW_LOGE("Strip LED count exceeds max (%u > %u)", config.ledCount, kMaxLedsPerStrip);
        return false;
    }

#ifndef NATIVE_BUILD
    constexpr uint8_t kStripPin = chip::gpio::LED_STRIP1_DATA;
    if (config.dataPin != kStripPin) {
        LW_LOGW("Strip pin override ignored (cfg=%u, hw=%u)", config.dataPin, kStripPin);
    }

    m_ctrl1 = &FastLED.addLeds<WS2812, kStripPin, GRB>(m_strip1, config.ledCount);
    applyColorCorrection(config);
    FastLED.setDither(1);
    FastLED.setMaxRefreshRate(0, true);
    setBrightness(config.brightness);
    setMaxPower(5, 3000);
    FastLED.clear(true);
#endif

    m_initialized = true;
    LW_LOGI("FastLED init: %u LEDs on GPIO %u", config.ledCount, chip::gpio::LED_STRIP1_DATA);
    return true;
}

bool LedDriver_S3::initDual(const LedStripConfig& config1, const LedStripConfig& config2) {
    m_dual = true;
    m_config1 = config1;
    m_config2 = config2;
    m_stripCounts[0] = config1.ledCount;
    m_stripCounts[1] = config2.ledCount;
    m_totalLeds = config1.ledCount + config2.ledCount;

    if (config1.ledCount > kMaxLedsPerStrip || config2.ledCount > kMaxLedsPerStrip) {
        LW_LOGE("Strip LED count exceeds max (%u/%u > %u)",
                config1.ledCount, config2.ledCount, kMaxLedsPerStrip);
        return false;
    }

#ifndef NATIVE_BUILD
    constexpr uint8_t kStrip1Pin = chip::gpio::LED_STRIP1_DATA;
    constexpr uint8_t kStrip2Pin = chip::gpio::LED_STRIP2_DATA;
    if (config1.dataPin != kStrip1Pin || config2.dataPin != kStrip2Pin) {
        LW_LOGW("Strip pin override ignored (cfg=%u/%u, hw=%u/%u)",
                config1.dataPin, config2.dataPin, kStrip1Pin, kStrip2Pin);
    }

    m_ctrl1 = &FastLED.addLeds<WS2812, kStrip1Pin, GRB>(m_strip1, config1.ledCount);
    m_ctrl2 = &FastLED.addLeds<WS2812, kStrip2Pin, GRB>(m_strip2, config2.ledCount);

    applyColorCorrection(config1);
    FastLED.setDither(1);
    FastLED.setMaxRefreshRate(0, true);
    setBrightness(config1.brightness);
    setMaxPower(5, 3000);
    FastLED.clear(true);
#endif

    m_initialized = true;
    LW_LOGI("FastLED init: 2x%u LEDs on GPIO %u/%u", config1.ledCount, kStrip1Pin, kStrip2Pin);
    return true;
}

void LedDriver_S3::deinit() {
#ifndef NATIVE_BUILD
    FastLED.clear(true);
#endif
    m_initialized = false;
}

CRGB* LedDriver_S3::getBuffer() {
    return m_strip1;
}

CRGB* LedDriver_S3::getBuffer(uint8_t stripIndex) {
    if (stripIndex == 0) return m_strip1;
    if (stripIndex == 1) return m_strip2;
    return nullptr;
}

uint16_t LedDriver_S3::getTotalLedCount() const {
    return m_totalLeds;
}

uint16_t LedDriver_S3::getLedCount(uint8_t stripIndex) const {
    return (stripIndex < 2) ? m_stripCounts[stripIndex] : 0;
}

void LedDriver_S3::show() {
#ifndef NATIVE_BUILD
    uint32_t start = static_cast<uint32_t>(esp_timer_get_time());
    FastLED.show();
    uint32_t end = static_cast<uint32_t>(esp_timer_get_time());
    updateShowStats(end - start);
#else
    m_stats.frameCount++;
#endif
}

void LedDriver_S3::setBrightness(uint8_t brightness) {
    m_brightness = brightness;
    m_stats.currentBrightness = brightness;
#ifndef NATIVE_BUILD
    FastLED.setBrightness(brightness);
#endif
}

uint8_t LedDriver_S3::getBrightness() const {
    return m_brightness;
}

void LedDriver_S3::setMaxPower(uint8_t volts, uint16_t milliamps) {
#ifndef NATIVE_BUILD
    FastLED.setMaxPowerInVoltsAndMilliamps(volts, milliamps);
#endif
}

void LedDriver_S3::clear(bool showNow) {
    fill(CRGB::Black, showNow);
}

void LedDriver_S3::fill(CRGB color, bool showNow) {
    for (uint16_t i = 0; i < m_stripCounts[0]; ++i) {
        m_strip1[i] = color;
    }
    for (uint16_t i = 0; i < m_stripCounts[1]; ++i) {
        m_strip2[i] = color;
    }
    if (showNow) {
        show();
    }
}

void LedDriver_S3::setPixel(uint16_t index, CRGB color) {
    if (index < m_stripCounts[0]) {
        m_strip1[index] = color;
        return;
    }
    uint16_t strip2Index = index - m_stripCounts[0];
    if (strip2Index < m_stripCounts[1]) {
        m_strip2[strip2Index] = color;
    }
}

void LedDriver_S3::resetStats() {
    m_stats = LedDriverStats{};
}

void LedDriver_S3::updateShowStats(uint32_t showUs) {
    m_stats.frameCount++;
    m_stats.lastShowUs = showUs;
    if (showUs > m_stats.maxShowUs) {
        m_stats.maxShowUs = showUs;
    }
    if (m_stats.frameCount == 1) {
        m_stats.avgShowUs = showUs;
    } else {
        m_stats.avgShowUs = (m_stats.avgShowUs * 7 + showUs) / 8;
    }
}

void LedDriver_S3::applyColorCorrection(const LedStripConfig& config) {
#ifndef NATIVE_BUILD
    FastLED.setCorrection(config.colorCorrection);
#else
    (void)config;
#endif
}

} // namespace hal
} // namespace lightwaveos
