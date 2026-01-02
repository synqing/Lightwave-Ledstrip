// ============================================================================
// LedFeedback - RGB LED Status Indicators for M5ROTATE8
// ============================================================================
// Implementation file for LED feedback system.
// Ported from K1.8encoderS3, adapted for Tab5.encoder's Rotate8Transport.
// ============================================================================

#include "LedFeedback.h"
#include "../input/Rotate8Transport.h"
#include <math.h>

// Static constexpr definitions (C++11 ODR-use requirement)
constexpr LedColor LedFeedback::COLOR_CONNECTING;
constexpr LedColor LedFeedback::COLOR_CONNECTED;
constexpr LedColor LedFeedback::COLOR_DISCONNECTED;
constexpr LedColor LedFeedback::COLOR_RECONNECTING;
constexpr LedColor LedFeedback::COLOR_FLASH_VALUE;
constexpr LedColor LedFeedback::COLOR_FLASH_RESET;

LedFeedback::LedFeedback()
    : _transport(nullptr)
    , _status(ConnectionStatus::DISCONNECTED)
    , _brightness(255)
    , _breathStart(0)
    , _breathingEnabled(false)
{
    // Initialize flash states
    for (uint8_t i = 0; i < 8; i++) {
        _flash[i].startTime = 0;
        _flash[i].active = false;
        _flash[i].r = 0;
        _flash[i].g = 0;
        _flash[i].b = 0;
    }
}

void LedFeedback::begin(Rotate8Transport& transport) {
    _transport = &transport;

    // Turn off all LEDs initially
    allOff();

    // Start with disconnected status
    setStatus(ConnectionStatus::DISCONNECTED);
}

void LedFeedback::setStatus(ConnectionStatus status) {
    _status = status;

    // Enable breathing effect for CONNECTING and RECONNECTING states
    _breathingEnabled = (status == ConnectionStatus::CONNECTING ||
                         status == ConnectionStatus::RECONNECTING);

    if (_breathingEnabled) {
        // Reset breathing phase
        _breathStart = millis();
    } else {
        // Set status LED immediately for solid states
        LedColor color = getStatusColor(status);
        applyBrightness(color);
        setRawLed(8, color);
    }
}

void LedFeedback::flashEncoder(uint8_t index) {
    if (index >= 8) return;

    _flash[index].active = true;
    _flash[index].startTime = millis();
    _flash[index].r = COLOR_FLASH_VALUE.r;
    _flash[index].g = COLOR_FLASH_VALUE.g;
    _flash[index].b = COLOR_FLASH_VALUE.b;

    // Set LED immediately
    LedColor color(COLOR_FLASH_VALUE.r, COLOR_FLASH_VALUE.g, COLOR_FLASH_VALUE.b);
    applyBrightness(color);
    setRawLed(index, color);
}

void LedFeedback::flashReset(uint8_t index) {
    if (index >= 8) return;

    _flash[index].active = true;
    _flash[index].startTime = millis();
    _flash[index].r = COLOR_FLASH_RESET.r;
    _flash[index].g = COLOR_FLASH_RESET.g;
    _flash[index].b = COLOR_FLASH_RESET.b;

    // Set LED immediately
    LedColor color(COLOR_FLASH_RESET.r, COLOR_FLASH_RESET.g, COLOR_FLASH_RESET.b);
    applyBrightness(color);
    setRawLed(index, color);
}

void LedFeedback::update() {
    if (!_transport) return;

    uint32_t now = millis();
    updateBreathing(now);
    updateFlash(now);
}

void LedFeedback::allOff() {
    if (!_transport) return;
    _transport->allLEDsOff();
}

void LedFeedback::setBrightness(uint8_t brightness) {
    _brightness = brightness;

    // Update status LED with new brightness
    if (!_breathingEnabled) {
        LedColor color = getStatusColor(_status);
        applyBrightness(color);
        setRawLed(8, color);
    }
}

void LedFeedback::updateBreathing(uint32_t now) {
    if (!_breathingEnabled || !_transport) return;

    // Get base color for current status
    LedColor baseColor = getStatusColor(_status);

    // Calculate breathing intensity using sine wave
    uint32_t elapsed = now - _breathStart;
    uint8_t intensity = breathValue(elapsed);

    // Apply breathing intensity (30% to 100% range)
    // Scale from 0-255 intensity to 30-100% brightness
    uint8_t scaledIntensity = 77 + ((intensity * 178) / 255);  // 77 = 30%, 255 = 100%

    LedColor breathColor;
    breathColor.r = (baseColor.r * scaledIntensity) / 255;
    breathColor.g = (baseColor.g * scaledIntensity) / 255;
    breathColor.b = (baseColor.b * scaledIntensity) / 255;

    applyBrightness(breathColor);
    setRawLed(8, breathColor);
}

void LedFeedback::updateFlash(uint32_t now) {
    if (!_transport) return;

    for (uint8_t i = 0; i < 8; i++) {
        if (_flash[i].active) {
            uint32_t elapsed = now - _flash[i].startTime;

            if (elapsed >= FLASH_DURATION_MS) {
                // Flash complete - turn LED off
                _flash[i].active = false;
                setRawLed(i, LedColor(0, 0, 0));
            }
            // While active, LED remains at flash color (set in flashEncoder/flashReset)
        }
    }
}

uint8_t LedFeedback::breathValue(uint32_t elapsed) {
    // Sine-wave breathing: 0-255-0 over BREATH_PERIOD_MS
    // Using float for smooth animation
    float phase = (float)(elapsed % BREATH_PERIOD_MS) / (float)BREATH_PERIOD_MS;
    float radians = phase * 2.0f * M_PI;
    float sineValue = (sinf(radians) + 1.0f) / 2.0f;  // 0.0 to 1.0
    return (uint8_t)(sineValue * 255.0f);
}

LedColor LedFeedback::getStatusColor(ConnectionStatus status) const {
    switch (status) {
        case ConnectionStatus::CONNECTING:
            return COLOR_CONNECTING;
        case ConnectionStatus::CONNECTED:
            return COLOR_CONNECTED;
        case ConnectionStatus::DISCONNECTED:
            return COLOR_DISCONNECTED;
        case ConnectionStatus::RECONNECTING:
            return COLOR_RECONNECTING;
        default:
            return COLOR_DISCONNECTED;  // Default to red (error)
    }
}

void LedFeedback::applyBrightness(LedColor& color) const {
    if (_brightness == 255) return;  // No adjustment needed

    // Scale each color component by brightness
    color.r = ((uint16_t)color.r * _brightness) / 255;
    color.g = ((uint16_t)color.g * _brightness) / 255;
    color.b = ((uint16_t)color.b * _brightness) / 255;
}

void LedFeedback::setRawLed(uint8_t index, const LedColor& color) {
    if (!_transport || index > 8) return;
    _transport->setLED(index, color.r, color.g, color.b);
}
