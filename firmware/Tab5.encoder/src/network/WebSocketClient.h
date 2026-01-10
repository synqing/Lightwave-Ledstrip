#pragma once
// ============================================================================
// WebSocketClient - Tab5.encoder
// ============================================================================
// Bidirectional WebSocket client for LightwaveOS communication.
// Ported from K1.8encoderS3 with Tab5-specific extensions (zone support).
//
// ESP32-P4 uses ESP32-C6 co-processor for WiFi via SDIO.
// Requires WiFi.setPins() before WiFi.begin() - see Config.h for pin defs.
// ============================================================================

#include "config/Config.h"

#if ENABLE_WIFI

#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config/network_config.h"
#include "../zones/ZoneDefinition.h"

// State Machine:
//   DISCONNECTED -> CONNECTING -> CONNECTED -> ERROR
//        ^              |             |          |
//        |              v             v          v
//        +-------------+-------------+-----------+
//
// Key Features:
//   - Non-blocking update() for main loop integration
//   - Exponential backoff reconnection (1s initial, 30s max)
//   - Per-parameter rate limiting (50ms throttle)
//   - Deferred hello message pattern
//   - Zone-specific command support (Tab5 extension)
//   - Uses constants from network_config.h
//
// WebSocket Protocol (LightwaveOS v2):
//   Outbound:
//     {"type": "effects.setCurrent", "effectId": N}
//     {"type": "parameters.set", "brightness": N, ...}
//     {"type": "zone.setEffect", "zoneId": N, "effectId": N}
//     {"type": "zone.setBrightness", "zoneId": N, "value": N}
//     {"type": "getStatus"}
//
//   Inbound:
//     {"type": "status", ...} - Full state sync
//     {"type": "effect.changed", ...} - Effect updates
//     {"type": "parameters.updated", ...} - Parameter changes

enum class WebSocketStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

// Callback type for received messages
// Uses JsonDocument (ArduinoJson v7+) for flexible document handling
// Note: ArduinoJson 7 deprecated StaticJsonDocument in favor of JsonDocument
using WebSocketMessageCallback = std::function<void(JsonDocument& doc)>;

// ============================================================================
// Color Correction State (cached from LightwaveOS v2 server)
// ============================================================================
// Populated by colorCorrection.getConfig response, used by PresetManager
// to capture/apply gamma, auto-exposure, and brown guardrail settings.

struct ColorCorrectionState {
    bool gammaEnabled = true;
    float gammaValue = 2.2f;
    bool autoExposureEnabled = false;
    uint8_t autoExposureTarget = 110;
    bool brownGuardrailEnabled = false;
    uint8_t maxGreenPercentOfRed = 28;
    uint8_t maxBluePercentOfRed = 8;
    uint8_t mode = 2;  // 0=OFF, 1=HSV, 2=RGB, 3=BOTH
    bool valid = false;  // True after first sync from server
};

class WebSocketClient {
public:
    WebSocketClient();

    // Initialize WebSocket connection to host (by hostname)
    // @param host Hostname (e.g., "lightwaveos.local")
    // @param port Port number (default: 80)
    // @param path WebSocket path (default: "/ws")
    void begin(const char* host, uint16_t port = 80, const char* path = "/ws");

    // Initialize WebSocket connection to host (by IP address)
    // @param ip IP address
    // @param port Port number (default: 80)
    // @param path WebSocket path (default: "/ws")
    void begin(IPAddress ip, uint16_t port = 80, const char* path = "/ws");

    // Update WebSocket connection and handle events (call from loop())
    // Non-blocking: returns immediately, handles state transitions internally
    void update();

    // Get connection status
    WebSocketStatus getStatus() const { return _status; }
    bool isConnected() const { return _status == WebSocketStatus::CONNECTED; }
    bool isConnecting() const { return _status == WebSocketStatus::CONNECTING; }

    // Get reconnect delay (for observability/debugging)
    unsigned long getReconnectDelay() const { return _reconnectDelay; }

    // ========================================================================
    // Global Parameter Commands (Unit A, encoders 0-7)
    // ========================================================================

    void sendEffectChange(uint8_t effectId);
    void sendPaletteChange(uint8_t paletteId);
    void sendSpeedChange(uint8_t speed);
    void sendMoodChange(uint8_t mood);
    void sendFadeAmountChange(uint8_t fadeAmount);
    void sendBrightnessChange(uint8_t brightness);
    void sendComplexityChange(uint8_t complexity);
    void sendVariationChange(uint8_t variation);

    // ========================================================================
    // Zone Commands (Tab5 extension for Unit B, encoders 8-15)
    // ========================================================================

    void sendZoneEnable(bool enable);
    void sendZoneEffect(uint8_t zoneId, uint8_t effectId);
    void sendZoneBrightness(uint8_t zoneId, uint8_t value);
    void sendZoneSpeed(uint8_t zoneId, uint8_t value);
    void sendZonePalette(uint8_t zoneId, uint8_t paletteId);
    void sendZoneBlend(uint8_t zoneId, uint8_t blendMode);
    void sendZonesSetLayout(const struct zones::ZoneSegment* segments, uint8_t zoneCount);

    // ========================================================================
    // Color Correction Commands
    // ========================================================================

    void requestColorCorrectionConfig();
    void sendColorCorrectionConfig(bool gammaEnabled, float gammaValue,
                                   bool autoExposureEnabled, uint8_t autoExposureTarget,
                                   bool brownGuardrailEnabled, uint8_t mode = 2);
    void sendGammaChange(bool enabled, float value);
    void sendAutoExposureChange(bool enabled, uint8_t target);
    void sendBrownGuardrailChange(bool enabled);
    void sendColourCorrectionMode(uint8_t mode);

    // Color correction state accessors
    const ColorCorrectionState& getColorCorrectionState() const { return _colorCorrectionState; }
    void setColorCorrectionState(const ColorCorrectionState& state) { _colorCorrectionState = state; }

    // ========================================================================
    // Generic Commands
    // ========================================================================

    void sendGenericParameter(const char* fieldName, uint8_t value);

    // ========================================================================
    // Metadata Requests (names for UI)
    // ========================================================================
    // These request lists from LightwaveOS. Responses come back as WebSocket
    // messages with the same "type" (e.g. "effects.list", "palettes.list").
    void requestEffectsList(uint8_t page, uint8_t limit, const char* requestId);
    void requestPalettesList(uint8_t page, uint8_t limit, const char* requestId);
    void requestZonesState();

    // ========================================================================
    // Message Handling
    // ========================================================================

    void onMessage(WebSocketMessageCallback callback) { _messageCallback = callback; }
    void disconnect();
    const char* getStatusString() const;

private:
    WebSocketsClient _ws;
    WebSocketStatus _status;
    WebSocketMessageCallback _messageCallback;

    // Color correction state (cached from server)
    ColorCorrectionState _colorCorrectionState;

    // Reconnection state
    unsigned long _lastReconnectAttempt;
    unsigned long _reconnectDelay;
    bool _shouldReconnect;
    IPAddress _serverIP;
    const char* _serverHost;
    uint16_t _serverPort;
    const char* _serverPath;
    bool _useIP;
    bool _pendingHello;

    // Rate limiting state (16 parameters for dual encoder units)
    struct RateLimiter {
        unsigned long lastSend[16];
    };
    RateLimiter _rateLimiter;

    // Send queue for parameter changes (prevents blocking on rapid encoder changes)
    struct PendingMessage {
        uint8_t paramIndex;
        uint8_t value;
        uint8_t zoneId;  // For zone parameters (0-3), unused for global params
        uint32_t timestamp;
        const char* type;
        bool valid;
        
        void reset() {
            valid = false;
            paramIndex = 0;
            value = 0;
            zoneId = 0;
            timestamp = 0;
            type = nullptr;
        }
    };
    static constexpr size_t SEND_QUEUE_SIZE = 16;  // One per parameter
    PendingMessage _sendQueue[SEND_QUEUE_SIZE];
    uint32_t _consecutiveSendFailures;
    bool _sendDegraded;

    // Mutex protection for _jsonBuffer (prevents concurrent access corruption)
    SemaphoreHandle_t _sendMutex;
    static constexpr uint32_t SEND_MUTEX_TIMEOUT_MS = 10;  // 10ms max wait
    static constexpr uint32_t SEND_TIMEOUT_MS = 50;  // 50ms max send time
    uint32_t _sendAttemptStartTime;

    // Parameter indices for rate limiting
    // Note: Unit B (8-15) encoders are disabled, but zone functions may still be called from UI
    enum ParamIndex : uint8_t {
        EFFECT = 0, BRIGHTNESS = 1, PALETTE = 2, SPEED = 3,
        MOOD = 4, FADEAMOUNT = 5, COMPLEXITY = 6, VARIATION = 7,
        ZONE0_EFFECT = 8, ZONE0_SPEED = 9,
        ZONE1_EFFECT = 10, ZONE1_SPEED = 11,
        ZONE2_EFFECT = 12, ZONE2_SPEED = 13,
        ZONE3_EFFECT = 14, ZONE3_SPEED = 15
    };

    // Fixed buffer for JSON serialization
    static constexpr size_t JSON_BUFFER_SIZE = 256;
    char _jsonBuffer[JSON_BUFFER_SIZE];

    void handleEvent(WStype_t type, uint8_t* payload, size_t length);
    void attemptReconnect();
    void resetReconnectBackoff();
    void increaseReconnectBackoff();
    void sendHelloMessage();
    
    // Check if parameter send is allowed (rate limiting)
    bool canSend(uint8_t paramIndex);
    
    // Send JSON message (non-blocking, protected by mutex)
    void sendJSON(const char* type, JsonDocument& doc);
    
    // Queue parameter change for later sending (prevents blocking)
    void queueParameterChange(uint8_t paramIndex, uint8_t value, const char* type, uint8_t zoneId = 255);
    
    // Process send queue (called from update())
    void processSendQueue();
    
    // Mutex helpers
    bool takeSendLock(uint32_t timeoutMs = SEND_MUTEX_TIMEOUT_MS);
    void releaseSendLock();
};

#endif // ENABLE_WIFI
