#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "config/features.h"

#if FEATURE_WEB_SERVER

#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <cJSON.h>

#include "IdfHttpServer.h"
#include "../config/network_config.h"

/**
 * LightwaveWebServer
 *
 * Robust web control plane (REST + WebSocket) using ESP-IDF `esp_http_server`.
 *
 * Notes:
 * - Compiled only when FEATURE_WEB_SERVER=1 (WiFi environments).
 * - JSON is cJSON only.
 */
class LightwaveWebServer {
public:
    LightwaveWebServer();
    ~LightwaveWebServer();

    bool begin();
    void stop();

    // Update functions (call from main loop)
    void update();
    void sendLEDUpdate();

    // Connection status
    bool hasClients() const { return m_wsClientCount > 0; }
    size_t getClientCount() const { return m_wsClientCount; }

    // Notifications
    void notifyEffectChange(uint8_t effectId);
    void notifyError(const String& message);

    // WebSocket broadcast (public for v2 handlers)
    void broadcastWs(const char* msg, size_t len) { m_http.wsBroadcastText(msg, len); }
    void sendWsToClient(int clientFd, const char* msg, size_t len) { m_http.wsSendText(clientFd, msg, len); }

private:
    IdfHttpServer m_http;
    size_t m_wsClientCount = 0;

    bool m_running = false;
    bool m_mdnsStarted = false;
    uint32_t m_lastHeartbeatMs = 0;

    void startMDNS();
    void stopMDNS();

    // Route registration (minimal in Phase 2; expanded in subsequent todos)
    void registerRoutes();

    // WebSocket handlers
    static void onWsClientEvent(int clientFd, bool connected, void* ctx);
    static void onWsMessage(int clientFd, const char* json, size_t len, void* ctx);

    // REST handlers
    static esp_err_t handleApiDiscovery(httpd_req_t* req);
    static esp_err_t handleOpenApi(httpd_req_t* req);
    static esp_err_t handleDeviceStatus(httpd_req_t* req);
    static esp_err_t handleEffectsList(httpd_req_t* req);
    static esp_err_t handleEffectsCurrent(httpd_req_t* req);
    static esp_err_t handleStaticRoot(httpd_req_t* req);
    static esp_err_t handleStaticAsset(httpd_req_t* req);
};

extern LightwaveWebServer webServer;

#endif // FEATURE_WEB_SERVER

#endif // WEB_SERVER_H


