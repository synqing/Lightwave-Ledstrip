#pragma once
// ============================================================================
// LedFeedback - Connection Status LED Feedback for Tab5.encoder
// ============================================================================
// Phase 4 (F.5): Connection status feedback using M5ROTATE8 status LEDs.
// Uses LED 8 on BOTH Unit A and Unit B to show connection state.
//
// Connection States:
//   WIFI_DISCONNECTED  - Solid red      (no WiFi connection)
//   WIFI_CONNECTING    - Blue breathing (WiFi connecting)
//   WIFI_CONNECTED     - Solid blue     (WiFi up, brief before WS)
//   WS_CONNECTING      - Yellow breathing (WebSocket connecting)
//   WS_CONNECTED       - Solid green    (fully connected)
//   WS_RECONNECTING    - Orange breathing (WebSocket reconnecting)
//
// Breathing Animation (K1 pattern):
//   Uses sine wave: brightness = base + amplitude * sin(millis() * 2 * PI / period)
//   Period: ~1500ms for natural breathing rhythm
//   Range: 30% to 100% brightness
//
// Reference: K1.8encoderS3 LedFeedback pattern adapted for dual encoder setup.
// ============================================================================

#ifdef SIMULATOR_BUILD
    #include <cstdint>
#else
    #include <Arduino.h>
#endif

// Forward declaration
class DualEncoderService;

// Connection status states (ordered by connection progression)
enum class ConnectionState : uint8_t {
    WIFI_DISCONNECTED = 0,   // Solid red - no WiFi
    WIFI_CONNECTING,         // Blue breathing - WiFi connecting
    WIFI_CONNECTED,          // Solid blue (brief) - WiFi up, pre-WS
    WS_CONNECTING,           // Yellow breathing - WebSocket connecting
    WS_CONNECTED,            // Solid green - fully connected
    WS_RECONNECTING          // Orange breathing - WebSocket reconnecting
};

// RGB color structure (compact, no dynamic allocation)
struct StatusLedColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    constexpr StatusLedColor() : r(0), g(0), b(0) {}
    constexpr StatusLedColor(uint8_t red, uint8_t green, uint8_t blue)
        : r(red), g(green), b(blue) {}
};

class LedFeedback {
public:
    // Constructor - requires DualEncoderService for LED control
    explicit LedFeedback(DualEncoderService* encoders);

    // Default constructor (encoders set later via setEncoders)
    LedFeedback();

    // Set encoder service reference (alternative to constructor)
    void setEncoders(DualEncoderService* encoders);

    // Initialize LED feedback (turns off all status LEDs)
    void begin();

    // Set connection state (updates LED color/animation)
    void setState(ConnectionState state);

    // Get current connection state
    ConnectionState getState() const { return m_state; }

    // Update LED animations (call in main loop)
    // Non-blocking: handles breathing animation timing internally
    void update();

    // Turn off both status LEDs
    void allOff();

    // Get status as human-readable string (for debugging)
    const char* getStateString() const;

private:
    DualEncoderService* m_encoders;
    ConnectionState m_state;

    // Animation state
    uint32_t m_animationStartTime;
    bool m_isBreathing;

    // Cached base color for current state
    StatusLedColor m_baseColor;

    // Timing constants
    static constexpr uint32_t BREATHING_PERIOD_MS = 1500;  // Full breath cycle
    static constexpr uint8_t BREATHING_MIN_PERCENT = 30;   // 30% min brightness
    static constexpr uint8_t BREATHING_MAX_PERCENT = 100;  // 100% max brightness

    // State-to-color mapping
    static StatusLedColor getColorForState(ConnectionState state);
    static bool stateRequiresBreathing(ConnectionState state);

    // Apply LED color to both units (Unit A LED 8 and Unit B LED 8)
    void applyColorToBothUnits(const StatusLedColor& color);

    // Calculate breathing brightness factor (0.30 - 1.0)
    float calculateBreathingFactor() const;
};
