#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

// Web server configuration - use values from NetworkConfig (see config/network_config.h)
// Avoid macro redefinitions by not defining new macros here.

class LightwaveWebServer {
private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;
    
    // Connection state
    bool isConnected = false;
    bool isRunning = false;
    bool shouldReboot = false;
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

#endif // WEB_SERVER_H 