#pragma once
// ============================================================================
// WebSocketClient - Stub for Tab5.encoder
// ============================================================================
// TODO (Milestone E): Implement full WebSocket client
// This stub provides the interface that ParameterHandler expects, allowing
// the firmware to compile without network functionality.
// ============================================================================

#include <Arduino.h>

class WebSocketClient {
public:
    WebSocketClient() = default;

    // Connection management (stub - always returns false)
    bool begin(const char* host, uint16_t port = 80, const char* path = "/ws") { return false; }
    void loop() {}
    bool isConnected() const { return false; }

    // Parameter change notifications (stub - do nothing)
    void sendEffectChange(uint8_t value) {}
    void sendBrightnessChange(uint8_t value) {}
    void sendPaletteChange(uint8_t value) {}
    void sendSpeedChange(uint8_t value) {}
    void sendIntensityChange(uint8_t value) {}
    void sendSaturationChange(uint8_t value) {}
    void sendComplexityChange(uint8_t value) {}
    void sendVariationChange(uint8_t value) {}

    // Generic parameter (for Unit B params 8-15)
    void sendGenericParameter(const char* fieldName, uint8_t value) {}
};
