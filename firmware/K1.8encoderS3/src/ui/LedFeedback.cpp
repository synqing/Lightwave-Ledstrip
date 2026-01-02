#include "LedFeedback.h"

LedFeedback::LedFeedback(M5ROTATE8& encoder)
    : _encoder(encoder)
    , _brightness(255)
    , _currentStatus(ConnectionStatus::DISCONNECTED)
    , _lastBreathUpdate(0)
    , _breathPhase(0)
    , _breathingEnabled(false)
{
    // Initialize palette colors to black
    for (uint8_t i = 0; i < 8; i++) {
        _paletteColors[i] = K1Color(0, 0, 0);
    }

    // Initialize status color
    _statusColor = getStatusColor(_currentStatus);

    // Initialize flash state
    _flash.active = false;
    _flash.encoderIndex = 0;
    _flash.startTime = 0;
    _flash.flashCount = 0;
}

bool LedFeedback::begin() {
    // Turn off all LEDs initially
    allOff();
    return true;
}

void LedFeedback::setLed(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    setLed(index, K1Color(r, g, b));
}

void LedFeedback::setLed(uint8_t index, const K1Color& color) {
    if (index > 8) return; // Invalid index

    K1Color adjusted = color;
    applyBrightness(adjusted);

    if (index < 8) {
        // Update palette color storage
        _paletteColors[index] = color;
    } else {
        // Update status color storage
        _statusColor = color;
    }

    setRawLed(index, adjusted);
}

void LedFeedback::setStatus(ConnectionStatus status) {
    _currentStatus = status;
    _statusColor = getStatusColor(status);

    // Enable breathing effect for CONNECTING and RECONNECTING states
    _breathingEnabled = (status == ConnectionStatus::CONNECTING ||
                        status == ConnectionStatus::RECONNECTING);

    // If not breathing, set LED immediately
    if (!_breathingEnabled) {
        K1Color adjusted = _statusColor;
        applyBrightness(adjusted);
        setRawLed(8, adjusted);
    }
}

void LedFeedback::setPaletteColors(const K1Color colors[8]) {
    for (uint8_t i = 0; i < 8; i++) {
        _paletteColors[i] = colors[i];

        // Don't update if this LED is currently flashing
        if (_flash.active && _flash.encoderIndex == i) {
            continue;
        }

        K1Color adjusted = colors[i];
        applyBrightness(adjusted);
        setRawLed(i, adjusted);
    }
}

void LedFeedback::flashEncoder(uint8_t encoderIndex) {
    if (encoderIndex >= 8) return; // Invalid encoder index

    _flash.active = true;
    _flash.encoderIndex = encoderIndex;
    _flash.startTime = millis();
    _flash.flashCount = 0;
}

void LedFeedback::setBrightness(uint8_t brightness) {
    _brightness = brightness;

    // Update all LEDs with new brightness
    for (uint8_t i = 0; i < 8; i++) {
        K1Color adjusted = _paletteColors[i];
        applyBrightness(adjusted);
        setRawLed(i, adjusted);
    }

    // Update status LED
    K1Color statusAdjusted = _statusColor;
    applyBrightness(statusAdjusted);
    setRawLed(8, statusAdjusted);
}

void LedFeedback::update() {
    updateBreathingEffect();
    updateFlashEffect();
}

void LedFeedback::allOff() {
    for (uint8_t i = 0; i < 9; i++) {
        _encoder.writeRGB(i, 0, 0, 0);
    }
}

void LedFeedback::updateBreathingEffect() {
    if (!_breathingEnabled) return;

    unsigned long now = millis();
    if (now - _lastBreathUpdate < BREATH_INTERVAL_MS) return;

    _lastBreathUpdate = now;

    // Use sine wave for smooth breathing
    // Phase goes 0-255, creating a full cycle
    _breathPhase = (_breathPhase + 2) % 256;

    // Calculate breathing intensity using sine approximation
    // Map 0-255 phase to 0-359 degrees, then to 30-100% brightness
    float radians = (_breathPhase / 255.0f) * 2.0f * PI;
    float sineValue = (sin(radians) + 1.0f) / 2.0f; // 0.0 to 1.0
    uint8_t breathIntensity = 30 + (uint8_t)(sineValue * 70); // 30% to 100%

    // Apply breathing to status LED
    K1Color breathColor = _statusColor;
    breathColor.r = (breathColor.r * breathIntensity) / 100;
    breathColor.g = (breathColor.g * breathIntensity) / 100;
    breathColor.b = (breathColor.b * breathIntensity) / 100;

    applyBrightness(breathColor);
    setRawLed(8, breathColor);
}

void LedFeedback::updateFlashEffect() {
    if (!_flash.active) return;

    unsigned long elapsed = millis() - _flash.startTime;

    if (elapsed >= FLASH_DURATION_MS) {
        // Flash complete - restore normal color
        _flash.active = false;

        K1Color adjusted = _paletteColors[_flash.encoderIndex];
        applyBrightness(adjusted);
        setRawLed(_flash.encoderIndex, adjusted);
    } else {
        // Flash in progress - boost brightness
        K1Color flashColor = _paletteColors[_flash.encoderIndex];

        // Add brightness boost
        uint16_t r = flashColor.r + FLASH_BRIGHTNESS_BOOST;
        uint16_t g = flashColor.g + FLASH_BRIGHTNESS_BOOST;
        uint16_t b = flashColor.b + FLASH_BRIGHTNESS_BOOST;

        // Clamp to 255
        flashColor.r = (r > 255) ? 255 : r;
        flashColor.g = (g > 255) ? 255 : g;
        flashColor.b = (b > 255) ? 255 : b;

        applyBrightness(flashColor);
        setRawLed(_flash.encoderIndex, flashColor);
    }
}

void LedFeedback::applyBrightness(K1Color& color) const {
    if (_brightness == 255) return; // No adjustment needed

    // Scale each color component by brightness
    color.r = ((uint16_t)color.r * _brightness) / 255;
    color.g = ((uint16_t)color.g * _brightness) / 255;
    color.b = ((uint16_t)color.b * _brightness) / 255;
}

K1Color LedFeedback::getStatusColor(ConnectionStatus status) const {
    switch (status) {
        case ConnectionStatus::CONNECTING:
            return K1Color(0, 0, 255);      // Blue
        case ConnectionStatus::CONNECTED:
            return K1Color(0, 255, 0);      // Green
        case ConnectionStatus::DISCONNECTED:
            return K1Color(255, 0, 0);      // Red
        case ConnectionStatus::RECONNECTING:
            return K1Color(255, 255, 0);    // Yellow
        default:
            return K1Color(255, 0, 0);      // Default to Red (error)
    }
}

void LedFeedback::setRawLed(uint8_t index, const K1Color& color) {
    // Use M5ROTATE8 writeRGB method
    // Note: M5ROTATE8 uses channels 0-7 for encoders, but library supports 0-8
    // The 9th LED (index 8) is accessed through the same method
    _encoder.writeRGB(index, color.r, color.g, color.b);
}
