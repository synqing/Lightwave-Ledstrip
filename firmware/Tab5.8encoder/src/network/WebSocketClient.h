#pragma once

#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <functional>

enum class WebSocketStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

// Callback type for received messages (uses StaticJsonDocument to avoid heap churn)
using WebSocketMessageCallback = std::function<void(StaticJsonDocument<512>& doc)>;

class WebSocketClient {
public:
    WebSocketClient();

    // Initialize WebSocket connection to host
    void begin(const char* host, uint16_t port = 80, const char* path = "/ws");
    void begin(IPAddress ip, uint16_t port = 80, const char* path = "/ws");

    // Update WebSocket connection and handle events
    void update();

    // Get connection status
    WebSocketStatus getStatus() const { return _status; }
    bool isConnected() const { return _status == WebSocketStatus::CONNECTED; }
    bool isConnecting() const { return _status == WebSocketStatus::CONNECTING; }
    
    // Get reconnect delay for observability
    unsigned long getReconnectDelay() const { return _reconnectDelay; }

    // Send sync commands
    void sendEffectChange(uint8_t effectId);
    void sendBrightnessChange(uint8_t brightness);
    void sendPaletteChange(uint8_t paletteId);
    void sendSpeedChange(uint8_t speed);
    void sendIntensityChange(uint8_t intensity);
    void sendSaturationChange(uint8_t saturation);
    void sendComplexityChange(uint8_t complexity);
    void sendVariationChange(uint8_t variation);

    // Set callback for received messages
    void onMessage(WebSocketMessageCallback callback) { _messageCallback = callback; }

    // Disconnect WebSocket
    void disconnect();

private:
    WebSocketsClient _ws;
    WebSocketStatus _status;
    WebSocketMessageCallback _messageCallback;

    // Reconnection state
    unsigned long _lastReconnectAttempt;
    unsigned long _reconnectDelay;
    bool _shouldReconnect;
    IPAddress _serverIP;
    const char* _serverHost;
    uint16_t _serverPort;
    const char* _serverPath;
    bool _useIP;
    bool _pendingHello; // Send hello message on next update() after connect

    // Rate limiting state
    struct RateLimiter {
        unsigned long lastSend[8];  // Track last send time for each parameter type
        static constexpr unsigned long THROTTLE_MS = 50;
    };
    RateLimiter _rateLimiter;

    // Parameter indices for rate limiting
    enum ParamIndex {
        EFFECT = 0,
        BRIGHTNESS = 1,
        PALETTE = 2,
        SPEED = 3,
        INTENSITY = 4,
        SATURATION = 5,
        COMPLEXITY = 6,
        VARIATION = 7
    };

    static constexpr unsigned long INITIAL_RECONNECT_DELAY_MS = 1000;
    static constexpr unsigned long MAX_RECONNECT_DELAY_MS = 30000;
    static constexpr unsigned long CONNECTION_TIMEOUT_MS = 20000; // 20 seconds for handshake

    // Internal methods
    void handleEvent(WStype_t type, uint8_t* payload, size_t length);
    void attemptReconnect();
    void resetReconnectBackoff();
    void increaseReconnectBackoff();
    bool canSend(ParamIndex param);
    void sendJSON(const char* type, StaticJsonDocument<128>& doc);
    void sendHelloMessage();
    
    // Fixed buffer for JSON serialization (no String allocations)
    static constexpr size_t JSON_BUFFER_SIZE = 256;
    char _jsonBuffer[JSON_BUFFER_SIZE];
};

