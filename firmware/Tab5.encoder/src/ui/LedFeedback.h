#pragma once
// ============================================================================
// LedFeedback - RGB LED Status Indicators for M5ROTATE8
// ============================================================================
// Provides visual feedback through the 9 RGB LEDs on M5ROTATE8:
//   - LEDs 0-7: One per encoder (activity flash on value change)
//   - LED 8: Status/center LED (connection status with breathing animation)
//
// Ported from K1.8encoderS3, adapted for Tab5.encoder's Rotate8Transport.
//
// Usage:
//   LedFeedback ledFeedback;
//   ledFeedback.begin(encoderService.transport());
//   ledFeedback.setStatus(ConnectionStatus::CONNECTED);
//   // In loop:
//   ledFeedback.update();
//
// Status LED Colors:
//   - CONNECTING:    Cyan (0, 128, 255) with breathing animation
//   - CONNECTED:     Green (0, 255, 0) solid
//   - DISCONNECTED:  Red (255, 0, 0) solid
//   - RECONNECTING:  Orange (255, 128, 0) with breathing animation
// ============================================================================

#include <Arduino.h>

// Forward declaration
class Rotate8Transport;

// Connection status for status LED (LED 8)
enum class ConnectionStatus : uint8_t {
    CONNECTING,    // Breathing cyan - WiFi/WebSocket connecting
    CONNECTED,     // Solid green - Connected and communicating
    DISCONNECTED,  // Solid red - Disconnected/error
    RECONNECTING   // Breathing orange - Attempting reconnect
};

// RGB color structure (constexpr-compatible for static const colors)
struct LedColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    constexpr LedColor() : r(0), g(0), b(0) {}
    constexpr LedColor(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
};

class LedFeedback {
public:
    LedFeedback();

    /**
     * Initialize LED feedback with transport reference
     * @param transport Reference to Rotate8Transport instance
     */
    void begin(Rotate8Transport& transport);

    /**
     * Set connection status (controls LED 8 behavior)
     * @param status Current connection status
     */
    void setStatus(ConnectionStatus status);

    /**
     * Flash encoder LED briefly (bright green) on value change
     * @param index Encoder index (0-7)
     */
    void flashEncoder(uint8_t index);

    /**
     * Flash encoder LED cyan on button reset
     * @param index Encoder index (0-7)
     */
    void flashReset(uint8_t index);

    /**
     * Update animations (call in main loop)
     * Handles breathing effect for status LED and flash timeouts
     */
    void update();

    /**
     * Turn off all LEDs (0-8)
     */
    void allOff();

    /**
     * Set LED brightness scaling (0-255)
     * @param brightness Global brightness multiplier
     */
    void setBrightness(uint8_t brightness);

    /**
     * Get current brightness level
     * @return Current brightness (0-255)
     */
    uint8_t getBrightness() const { return _brightness; }

    /**
     * Check if feedback system is initialized
     * @return true if begin() was called with valid transport
     */
    bool isInitialized() const { return _transport != nullptr; }

private:
    Rotate8Transport* _transport;

    // Connection status
    ConnectionStatus _status;

    // Global brightness (0-255)
    uint8_t _brightness;

    // Breathing animation state for status LED
    uint32_t _breathStart;
    bool _breathingEnabled;

    // Flash state per encoder (LEDs 0-7)
    struct FlashState {
        uint32_t startTime;
        bool active;
        uint8_t r, g, b;
    };
    FlashState _flash[8];

    // Animation timing constants
    static constexpr uint32_t BREATH_PERIOD_MS = 2000;    // Full breathing cycle
    static constexpr uint32_t FLASH_DURATION_MS = 100;    // Flash duration

    // Status LED colors
    static constexpr LedColor COLOR_CONNECTING   = LedColor(0, 128, 255);   // Cyan
    static constexpr LedColor COLOR_CONNECTED    = LedColor(0, 255, 0);     // Green
    static constexpr LedColor COLOR_DISCONNECTED = LedColor(255, 0, 0);     // Red
    static constexpr LedColor COLOR_RECONNECTING = LedColor(255, 128, 0);   // Orange

    // Flash colors
    static constexpr LedColor COLOR_FLASH_VALUE  = LedColor(0, 255, 0);     // Bright green
    static constexpr LedColor COLOR_FLASH_RESET  = LedColor(0, 128, 255);   // Cyan

    // Internal methods
    void updateBreathing(uint32_t now);
    void updateFlash(uint32_t now);
    uint8_t breathValue(uint32_t elapsed);
    LedColor getStatusColor(ConnectionStatus status) const;
    void applyBrightness(LedColor& color) const;
    void setRawLed(uint8_t index, const LedColor& color);
};
