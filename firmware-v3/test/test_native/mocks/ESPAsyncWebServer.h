/**
 * @file ESPAsyncWebServer.h
 * @brief ESPAsyncWebServer mock for native unit tests
 *
 * Provides minimal AsyncWebSocket, AsyncWebSocketClient, AsyncWebServer,
 * AsyncWebServerRequest, and AsyncWebServerResponse stubs for testing
 * WebSocket command routing without ESP32 hardware.
 *
 * Test instrumentation: clients record sent messages via m_lastMessage,
 * servers record registered routes via m_routes.
 */

#pragma once

#ifdef NATIVE_BUILD

#include "Arduino.h"
#include <vector>
#include <functional>

// Minimal IPAddress stub
struct IPAddress {
    uint8_t octets[4] = {192, 168, 4, 2};
    String toString() const { return "192.168.4.2"; }
};

// AsyncWebSocketClient — records text() calls for assertions
class AsyncWebSocketClient {
public:
    uint32_t id() const { return m_id; }
    IPAddress remoteIP() const { return m_ip; }

    void text(const char* msg) { m_lastMessage = msg; }
    void text(const String& msg) { m_lastMessage = msg; }
    void close() { m_closed = true; }

    // Test instrumentation
    uint32_t m_id = 1;
    IPAddress m_ip;
    String m_lastMessage;
    bool m_closed = false;
};

// AsyncWebSocket — records textAll() broadcasts
class AsyncWebSocket {
public:
    explicit AsyncWebSocket(const char* url = "/ws") : m_url(url) {}

    void textAll(const char* msg) { m_lastBroadcast = msg; }
    void textAll(const String& msg) { m_lastBroadcast = msg; }
    size_t count() const { return m_clientCount; }

    // Test instrumentation
    String m_lastBroadcast;
    size_t m_clientCount = 0;
    std::string m_url;
};

// AsyncWebServerResponse — stub for ApiResponse.h compilation
class AsyncWebServerResponse {
public:
    void addHeader(const char* name, const char* value) {
        (void)name; (void)value;
    }
};

// AsyncWebServerRequest — stub for ApiResponse.h compilation
class AsyncWebServerRequest {
public:
    void send(uint16_t code, const char* contentType, const String& content) {
        m_responseCode = code;
        m_responseBody = content;
    }
    void send(AsyncWebServerResponse* response) { (void)response; }

    AsyncWebServerResponse* beginResponse(uint16_t code, const char* contentType,
                                           const String& content) {
        m_responseCode = code;
        m_responseBody = content;
        return &m_mockResponse;
    }

    // Test instrumentation
    uint16_t m_responseCode = 0;
    String m_responseBody;
    AsyncWebServerResponse m_mockResponse;
};

// AsyncWebServer — stub (not needed by WsCommandRouter tests)
class AsyncWebServer {
public:
    explicit AsyncWebServer(uint16_t port) : m_port(port) {}
    void begin() {}

    uint16_t m_port;
};

#endif // NATIVE_BUILD
