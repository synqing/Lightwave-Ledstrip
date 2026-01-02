#ifndef LED_FEEDBACK_H
#define LED_FEEDBACK_H

#include <Arduino.h>
#include "M5ROTATE8.h"

// Connection status for status LED
enum class ConnectionStatus : uint8_t {
    CONNECTING,    // Blue - WiFi/WebSocket connecting
    CONNECTED,     // Green - Connected and communicating
    DISCONNECTED,  // Red - Disconnected/error
    RECONNECTING   // Yellow - Attempting reconnect
};

// RGB color structure (renamed to avoid M5GFX conflict)
struct K1Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    K1Color() : r(0), g(0), b(0) {}
    K1Color(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
};

class LedFeedback {
public:
    // Constructor - requires M5ROTATE8 instance
    LedFeedback(M5ROTATE8& encoder);

    // Initialize LED controller
    bool begin();

    // Set individual LED color (index 0-8)
    // LEDs 0-7: Encoder LEDs
    // LED 8: Status LED
    void setLed(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
    void setLed(uint8_t index, const K1Color& color);

    // Set status LED based on connection state
    void setStatus(ConnectionStatus status);

    // Update palette preview colors (LEDs 0-7)
    // colors array must contain 8 RGB values
    void setPaletteColors(const K1Color colors[8]);

    // Highlight active encoder with brief flash
    // Call when encoder value changes
    void flashEncoder(uint8_t encoderIndex);

    // Set overall LED brightness (0-255)
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const { return _brightness; }

    // Update animations (call in main loop)
    // Handles breathing effect and flash animations
    void update();

    // Turn off all LEDs
    void allOff();

private:
    M5ROTATE8& _encoder;

    // Current brightness level (0-255)
    uint8_t _brightness;

    // Current connection status
    ConnectionStatus _currentStatus;

    // Palette colors for LEDs 0-7
    K1Color _paletteColors[8];

    // Status LED base color
    K1Color _statusColor;

    // Animation state for breathing effect
    unsigned long _lastBreathUpdate;
    uint8_t _breathPhase;
    bool _breathingEnabled;

    // Flash animation state
    struct FlashState {
        bool active;
        uint8_t encoderIndex;
        unsigned long startTime;
        uint8_t flashCount;
    };
    FlashState _flash;

    // Animation timing constants
    static constexpr unsigned long BREATH_INTERVAL_MS = 20;
    static constexpr unsigned long FLASH_DURATION_MS = 150;
    static constexpr uint8_t FLASH_BRIGHTNESS_BOOST = 100;

    // Helper methods
    void updateBreathingEffect();
    void updateFlashEffect();
    void applyBrightness(K1Color& color) const;
    K1Color getStatusColor(ConnectionStatus status) const;
    void setRawLed(uint8_t index, const K1Color& color);
};

#endif // LED_FEEDBACK_H
