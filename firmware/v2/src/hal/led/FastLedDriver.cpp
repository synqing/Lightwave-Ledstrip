/**
 * @file FastLedDriver.cpp
 * @brief FastLED-based implementation of ILedDriver
 *
 * LightwaveOS v2 HAL - FastLED Driver Implementation
 *
 * @copyright 2024 LightwaveOS Project
 */

#include "FastLedDriver.h"

#ifndef NATIVE_BUILD
#include <FastLED.h>
#endif

#ifdef ESP32
#include <esp_timer.h>
#endif

#include <cstring>

namespace lightwaveos {
namespace hal {

// ============================================================================
// Construction / Destruction
// ============================================================================

FastLedDriver::FastLedDriver(const LedDriverConfig& config)
    : m_config(config)
    , m_buffer(nullptr)
    , m_totalLeds(0)
    , m_initialized(false)
    , m_brightness(config.defaultBrightness)
    , m_powerVoltage(config.powerVoltage)
    , m_powerMilliamps(config.totalPowerBudget)
    , m_lastShowTimeUs(0)
    , m_showCount(0)
    , m_totalShowTimeUs(0)
#ifdef ESP32
    , m_mutex(nullptr)
#endif
{
    // Initialize arrays to null/zero
    for (uint8_t i = 0; i < MAX_STRIPS; ++i) {
        m_stripBuffers[i] = nullptr;
        m_stripStarts[i] = 0;
        m_controllers[i] = nullptr;
    }

    // Calculate total LEDs
    m_totalLeds = config.getTotalLedCount();

    // Calculate strip start indices
    uint16_t currentStart = 0;
    for (uint8_t i = 0; i < config.stripCount; ++i) {
        m_stripStarts[i] = currentStart;
        currentStart += config.strips[i].ledCount;
    }
}

FastLedDriver::~FastLedDriver() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool FastLedDriver::init() {
    if (m_initialized) {
        return true;  // Already initialized
    }

    // Validate configuration
    if (!m_config.isValid()) {
        return false;
    }

#ifdef ESP32
    // Create mutex for thread safety
    m_mutex = xSemaphoreCreateMutex();
    if (m_mutex == nullptr) {
        return false;
    }
#endif

    // Allocate unified LED buffer
    // Using static allocation for embedded reliability
    // Memory: 320 LEDs * 3 bytes = 960 bytes
    static RGB s_ledBuffer[320];  // Max supported LEDs
    if (m_totalLeds > 320) {
        return false;  // Configuration exceeds static buffer
    }

    m_buffer = s_ledBuffer;
    std::memset(m_buffer, 0, m_totalLeds * sizeof(RGB));

    // Set up strip buffer pointers (pointing into unified buffer)
    for (uint8_t i = 0; i < m_config.stripCount; ++i) {
        m_stripBuffers[i] = &m_buffer[m_stripStarts[i]];
    }

#ifndef NATIVE_BUILD
    initializeFastLED();
#endif

    m_initialized = true;
    return true;
}

void FastLedDriver::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Turn off all LEDs
    clear();
#ifndef NATIVE_BUILD
    FastLED.show();
    FastLED.clear(true);
#endif

#ifdef ESP32
    // Clean up mutex
    if (m_mutex != nullptr) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
#endif

    // Reset state (buffer is static, don't free)
    m_buffer = nullptr;
    for (uint8_t i = 0; i < MAX_STRIPS; ++i) {
        m_stripBuffers[i] = nullptr;
        m_controllers[i] = nullptr;
    }

    m_initialized = false;
}

bool FastLedDriver::isReady() const {
    return m_initialized && (m_buffer != nullptr);
}

// ============================================================================
// Configuration
// ============================================================================

uint16_t FastLedDriver::getLedCount() const {
    return m_totalLeds;
}

uint16_t FastLedDriver::getCenterPoint() const {
    return m_config.centerPoint;
}

StripTopology FastLedDriver::getTopology() const {
    StripTopology topo;
    topo.totalLeds = m_totalLeds;
    topo.ledsPerStrip = m_config.stripCount > 0 ? m_config.strips[0].ledCount : 0;
    topo.stripCount = m_config.stripCount;
    topo.centerPoint = m_config.centerPoint;
    topo.halfLength = m_config.centerPoint;  // LEDs from start to center
    return topo;
}

// ============================================================================
// Buffer Operations
// ============================================================================

void FastLedDriver::setLed(uint16_t index, RGB color) {
    if (index >= m_totalLeds || m_buffer == nullptr) {
        return;  // Fail-safe: ignore out-of-range
    }
    m_buffer[index] = color;
}

void FastLedDriver::setLed(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    setLed(index, RGB(r, g, b));
}

RGB FastLedDriver::getLed(uint16_t index) const {
    if (index >= m_totalLeds || m_buffer == nullptr) {
        return RGB::Black();
    }
    return m_buffer[index];
}

void FastLedDriver::fill(RGB color) {
    if (m_buffer == nullptr) {
        return;
    }
    for (uint16_t i = 0; i < m_totalLeds; ++i) {
        m_buffer[i] = color;
    }
}

void FastLedDriver::fillRange(uint16_t startIndex, uint16_t count, RGB color) {
    if (m_buffer == nullptr) {
        return;
    }

    // Clamp range to valid indices
    if (startIndex >= m_totalLeds) {
        return;
    }
    if (startIndex + count > m_totalLeds) {
        count = m_totalLeds - startIndex;
    }

    for (uint16_t i = 0; i < count; ++i) {
        m_buffer[startIndex + i] = color;
    }
}

void FastLedDriver::clear() {
    if (m_buffer == nullptr) {
        return;
    }
    std::memset(m_buffer, 0, m_totalLeds * sizeof(RGB));
}

RGB* FastLedDriver::getBuffer() {
    return m_buffer;
}

const RGB* FastLedDriver::getBuffer() const {
    return m_buffer;
}

// ============================================================================
// Output Control
// ============================================================================

void FastLedDriver::show() {
    if (!m_initialized || m_buffer == nullptr) {
        return;
    }

#ifdef ESP32
    // Acquire mutex for thread-safe show
    if (m_mutex != nullptr) {
        if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            return;  // Could not acquire mutex, skip this frame
        }
    }

    uint32_t startTime = esp_timer_get_time();
#endif

#ifndef NATIVE_BUILD
    // Sync our RGB buffer to FastLED's CRGB buffers
    syncBuffersToFastLED();

    // Output to physical LEDs
    FastLED.show();
#endif

#ifdef ESP32
    uint32_t endTime = esp_timer_get_time();
    m_lastShowTimeUs = endTime - startTime;
    m_showCount++;
    m_totalShowTimeUs += m_lastShowTimeUs;

    if (m_mutex != nullptr) {
        xSemaphoreGive(m_mutex);
    }
#endif
}

void FastLedDriver::setBrightness(uint8_t brightness) {
    // Clamp to max brightness
    if (brightness > m_config.maxBrightness) {
        brightness = m_config.maxBrightness;
    }
    m_brightness = brightness;

#ifndef NATIVE_BUILD
    FastLED.setBrightness(m_brightness);
#endif
}

uint8_t FastLedDriver::getBrightness() const {
    return m_brightness;
}

void FastLedDriver::setMaxPower(uint8_t volts, uint32_t milliamps) {
    m_powerVoltage = volts;
    m_powerMilliamps = milliamps;

#ifndef NATIVE_BUILD
    FastLED.setMaxPowerInVoltsAndMilliamps(volts, milliamps);
#endif
}

// ============================================================================
// Performance
// ============================================================================

uint32_t FastLedDriver::getLastShowTime() const {
    return m_lastShowTimeUs;
}

float FastLedDriver::getEstimatedFPS() const {
    if (m_lastShowTimeUs == 0) {
        return 0.0f;
    }
    // FPS = 1,000,000 / microseconds per frame
    return 1000000.0f / static_cast<float>(m_lastShowTimeUs);
}

// ============================================================================
// FastLED-Specific
// ============================================================================

void* FastLedDriver::getController(uint8_t stripIndex) const {
    if (stripIndex >= m_config.stripCount) {
        return nullptr;
    }
    return m_controllers[stripIndex];
}

void FastLedDriver::setDithering(bool enable) {
#ifndef NATIVE_BUILD
    FastLED.setDither(enable ? 1 : 0);
#else
    (void)enable;
#endif
}

void FastLedDriver::setColorCorrection(uint32_t correction) {
#ifndef NATIVE_BUILD
    FastLED.setCorrection(CRGB(correction));
#else
    (void)correction;
#endif
}

bool FastLedDriver::getPhysicalStripBuffer(uint8_t stripIndex,
                                            RGB** buffer,
                                            uint16_t* count) const {
    if (stripIndex >= m_config.stripCount) {
        return false;
    }

    if (buffer != nullptr) {
        *buffer = m_stripBuffers[stripIndex];
    }
    if (count != nullptr) {
        *count = m_config.strips[stripIndex].ledCount;
    }
    return true;
}

// ============================================================================
// Private Helpers
// ============================================================================

void FastLedDriver::mapLogicalToPhysical(uint16_t logicalIndex,
                                          uint8_t& stripIndex,
                                          uint16_t& stripOffset) const {
    // Find which strip this logical index belongs to
    for (uint8_t i = 0; i < m_config.stripCount; ++i) {
        uint16_t stripEnd = m_stripStarts[i] + m_config.strips[i].ledCount;
        if (logicalIndex < stripEnd) {
            stripIndex = i;
            stripOffset = logicalIndex - m_stripStarts[i];

            // Handle reversed strips
            if (m_config.strips[i].reversed) {
                stripOffset = m_config.strips[i].ledCount - 1 - stripOffset;
            }
            return;
        }
    }

    // Fallback: invalid index
    stripIndex = 0;
    stripOffset = 0;
}

#ifndef NATIVE_BUILD

// Static FastLED buffers (outside the class to work with FastLED templates)
// These are the actual CRGB buffers FastLED uses
static CRGB s_fastled_strip1[160];
static CRGB s_fastled_strip2[160];

void FastLedDriver::initializeFastLED() {
    // Get pin assignments from config
    const StripConfig& strip1Config = m_config.strips[0];
    const StripConfig& strip2Config = m_config.strips[1];

    // Initialize FastLED with WS2812 on configured pins
    // Note: FastLED requires compile-time pin constants for templates,
    // but we can use addLeds() with runtime pins via the generic interface

    // For LightwaveOS v1 hardware: GPIO4 and GPIO5, GRB color order
    // Using the RMT driver for ESP32-S3
    if (m_config.stripCount >= 1 && strip1Config.ledCount <= 160) {
        m_controllers[0] = &FastLED.addLeds<WS2812, 4, GRB>(
            s_fastled_strip1, strip1Config.ledCount);
    }

    if (m_config.stripCount >= 2 && strip2Config.ledCount <= 160) {
        m_controllers[1] = &FastLED.addLeds<WS2812, 5, GRB>(
            s_fastled_strip2, strip2Config.ledCount);
    }

    // Configure FastLED global settings
    FastLED.setBrightness(m_brightness);
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setDither(m_config.enableDithering ? 1 : 0);
    FastLED.setMaxRefreshRate(0, true);  // Non-blocking mode
    FastLED.setMaxPowerInVoltsAndMilliamps(m_powerVoltage, m_powerMilliamps);

    // Clear all LEDs on init
    FastLED.clear(true);
}

void FastLedDriver::syncBuffersToFastLED() {
    // Copy from our RGB buffer to FastLED's CRGB buffers
    // This maintains abstraction while allowing FastLED to handle output

    if (m_config.stripCount >= 1 && m_stripBuffers[0] != nullptr) {
        const uint16_t count1 = m_config.strips[0].ledCount;
        for (uint16_t i = 0; i < count1; ++i) {
            const RGB& src = m_stripBuffers[0][i];
            s_fastled_strip1[i] = CRGB(src.r, src.g, src.b);
        }
    }

    if (m_config.stripCount >= 2 && m_stripBuffers[1] != nullptr) {
        const uint16_t count2 = m_config.strips[1].ledCount;
        for (uint16_t i = 0; i < count2; ++i) {
            const RGB& src = m_stripBuffers[1][i];
            s_fastled_strip2[i] = CRGB(src.r, src.g, src.b);
        }
    }
}

#else  // NATIVE_BUILD

void FastLedDriver::initializeFastLED() {
    // No-op for native builds (unit testing)
}

void FastLedDriver::syncBuffersToFastLED() {
    // No-op for native builds (unit testing)
}

#endif  // NATIVE_BUILD

} // namespace hal
} // namespace lightwaveos
