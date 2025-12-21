#pragma once

#include <Arduino.h>
#include <cJSON.h>
#include <vector>

extern "C" {
#include "esp_http_server.h"
}

/**
 * ESP-IDF native HTTP + WebSocket server wrapper (Arduino-ESP32 compatible).
 *
 * Design goals:
 * - Deterministic dependency graph (no ESPAsyncWebServer / ArduinoJson).
 * - Explicit memory ownership (bounded request bodies; cJSON create/free).
 * - REST + WebSocket support behind FEATURE_WEB_SERVER.
 */
class IdfHttpServer {
public:
    struct Config {
        uint16_t port = 80;
        size_t maxUriHandlers = 256;  // v2 API registers ~180 handlers (v1+v2+options)
        size_t maxOpenSockets = 8;
        size_t maxReqBodyBytes = 2048;     // hard limit for JSON bodies
        size_t maxWsFrameBytes = 2048;     // hard limit for WS frames
        bool enableLruPurge = true;
    };

    using WsMessageHandler = void (*)(int clientFd, const char* json, size_t len, void* ctx);
    using WsClientEventHandler = void (*)(int clientFd, bool connected, void* ctx);

    IdfHttpServer();
    ~IdfHttpServer();

    bool begin(const Config& cfg);
    void stop();
    bool isRunning() const { return m_server != nullptr; }

    httpd_handle_t handle() const { return m_server; }

    // WebSocket plumbing
    void setWsHandlers(WsClientEventHandler onClientEvent,
                       WsMessageHandler onMessage,
                       void* ctx);

    // Broadcast to all connected WS clients (best-effort).
    void wsBroadcastText(const char* msg, size_t len);

    // Send a text frame to a specific WS client (best-effort).
    void wsSendText(int clientFd, const char* msg, size_t len);

    // REST helpers
    bool registerGet(const char* uri, esp_err_t (*handler)(httpd_req_t* req));
    bool registerPost(const char* uri, esp_err_t (*handler)(httpd_req_t* req));
    bool registerPut(const char* uri, esp_err_t (*handler)(httpd_req_t* req));
    bool registerPatch(const char* uri, esp_err_t (*handler)(httpd_req_t* req));
    bool registerDelete(const char* uri, esp_err_t (*handler)(httpd_req_t* req));
    bool registerOptions(const char* uri, esp_err_t (*handler)(httpd_req_t* req));

    // Utility: read body into a bounded buffer (heap-allocated), null-terminated.
    // Caller must free() returned buffer.
    char* readBody(httpd_req_t* req, size_t* outLen) const;

    // Utility: send JSON response from cJSON root (serialised unformatted).
    esp_err_t sendJson(httpd_req_t* req, int statusCode, cJSON* root) const;

    // Default CORS OPTIONS handler (204 No Content).
    static esp_err_t corsOptionsHandler(httpd_req_t* req);
    static esp_err_t handle404(httpd_req_t* req, httpd_err_code_t err);

private:
    httpd_handle_t m_server = nullptr;
    Config m_cfg{};

    // WS handlers
    WsClientEventHandler m_onWsClientEvent = nullptr;
    WsMessageHandler m_onWsMessage = nullptr;
    void* m_wsCtx = nullptr;

    // WS client list
    static constexpr size_t MAX_WS_CLIENTS = 8;
    int m_wsClientFds[MAX_WS_CLIENTS];

    static void addCorsHeaders(httpd_req_t* req);
    void recordRoute(const char* method, const char* uri, bool ok);

    bool registerInternalHandlers();

    // WS endpoint handler (fixed URI: /ws)
    static esp_err_t wsHandler(httpd_req_t* req);
    void onWsConnect(int fd);
    void onWsDisconnect(int fd);
    void onWsData(httpd_req_t* req);

    void wsClientAdd(int fd);
    void wsClientRemove(int fd);
    bool wsClientHas(int fd) const;

    // Route registry for diagnostics
    std::vector<String> m_routes;
    static IdfHttpServer* s_lastInstance;
};


