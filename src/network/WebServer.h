#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "config/features.h"

#if FEATURE_WEB_SERVER

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

// v1 API infrastructure
#include "ApiResponse.h"
#include "RequestValidator.h"
#include "RateLimiter.h"
#include "ConnectionManager.h"

// Web server configuration - use values from NetworkConfig (see config/network_config.h)
// Avoid macro redefinitions by not defining new macros here.

class LightwaveWebServer {
private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;

    // v1 API infrastructure
    RateLimiter rateLimiter;
    ConnectionManager connectionMgr;

    // Connection state
    bool isConnected = false;
    bool isRunning = false;
    bool shouldReboot = false;
    bool mdnsStarted = false;
    uint32_t lastHeartbeat = 0;

    // JSON document size
    static constexpr size_t JSON_DOC_SIZE = 2048;
    
    // WebSocket event handler
    void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                         AwsEventType type, void* arg, uint8_t* data, size_t len);
    
    // Command handlers
    void handleCommand(AsyncWebSocketClient* client, const JsonDocument& doc);
    void handleSetParameter(const JsonDocument& doc);
    void handleSetEffect(const JsonDocument& doc);
    void handleSetPalette(const JsonDocument& doc);
    void handleTogglePower();
    void handleEmergencyStop();
    void handleSavePreset(const JsonDocument& doc);
    
    // State broadcast
    void broadcastState();
    void broadcastStatus();
    void broadcastZoneState();  // Broadcast zone config to all clients
    void broadcastPerformance();
    void broadcastLEDData();
    
    // Network status handlers
    void onWiFiConnected();
    void onAPModeStarted();
    void onWiFiDisconnected();
    void startWiFiMonitor();
    void startMDNS();
    bool beginNetworkServices();
    void setupRoutes();
    void setupV1Routes();  // v1 API routes

    // v1 API endpoint handlers
    void handleApiDiscovery(AsyncWebServerRequest* request);
    void handleDeviceStatus(AsyncWebServerRequest* request);
    void handleDeviceInfo(AsyncWebServerRequest* request);
    void handleEffectsList(AsyncWebServerRequest* request);
    void handleEffectsCurrent(AsyncWebServerRequest* request);
    void handleEffectMetadata(AsyncWebServerRequest* request);
    void handleEffectsSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleParametersGet(AsyncWebServerRequest* request);
    void handleParametersSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleTransitionTypes(AsyncWebServerRequest* request);
    void handleTransitionConfigGet(AsyncWebServerRequest* request);
    void handleTransitionConfigSet(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleTransitionTrigger(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void handleBatch(AsyncWebServerRequest* request, uint8_t* data, size_t len);

    // v1 helper methods
    bool checkRateLimit(AsyncWebServerRequest* request);
    bool executeBatchAction(const String& action, JsonVariant params);

    // Static WebSocket handler
    static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                          AwsEventType type, void* arg, uint8_t* data, size_t len);
    
    // OTA firmware update handler
    void handleOTAUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    
public:
    LightwaveWebServer();
    ~LightwaveWebServer();
    
    // Initialize and start server
    bool begin();
    void stop();
    
    // Update functions (call from main loop)
    void update();
    void sendLEDUpdate();
    
    // Connection status
    bool hasClients() const { return ws->count() > 0; }
    size_t getClientCount() const { return ws->count(); }
    
    // Send notifications
    void notifyEffectChange(uint8_t effectId);
    void notifyError(const String& message);
    
    // Get server instance for adding routes
    AsyncWebServer* getServer() { return server; }
};

// Global instance
extern LightwaveWebServer webServer;

#endif // FEATURE_WEB_SERVER

#endif // WEB_SERVER_H 