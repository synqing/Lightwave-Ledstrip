// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
#pragma once
// ============================================================================
// HttpClient - Tab5.encoder
// ============================================================================
// Simple HTTP client for making REST API calls to the v2 device.
// Uses WiFiClient for synchronous HTTP requests.
//
// NOTE: This is a lightweight client for the connectivity UI.
// For WebSocket communication, use WebSocketClient instead.
// ============================================================================

#include "config/Config.h"

#if ENABLE_WIFI

#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <IPAddress.h>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

/**
 * HTTP response structure
 */
struct HttpResponse {
    int statusCode = 0;
    String body;
    bool success = false;
    String errorMessage;
};

/**
 * Network entry structure (from v2 REST API)
 */
struct NetworkEntry {
    String ssid;
    String password;  // Empty if not available (for saved networks)
    bool isSaved = false;
};

/**
 * Scan result structure (from v2 REST API)
 */
struct ScanResult {
    String ssid;
    int32_t rssi = 0;
    uint8_t channel = 0;
    bool encrypted = false;
    String encryptionType;  // "WPA2", "WPA", "WEP", "Open"
};

/**
 * Scan job status (from v2 REST API)
 */
struct ScanStatus {
    bool inProgress = false;
    uint32_t jobId = 0;
    uint8_t networkCount = 0;
    ScanResult networks[20];  // Max 20 networks
};

/**
 * HTTP client for v2 device REST API
 */
class HttpClient {
public:
    enum class DiscoveryState : uint8_t {
        IDLE = 0,
        RUNNING,
        SUCCESS,
        FAILED
    };

    HttpClient();
    ~HttpClient();

    /**
     * Set the v2 device IP address
     * @param ip IP address of the v2 device
     */
    void setServerIP(IPAddress ip) { _serverIP = ip; }

    /**
     * Set the v2 device hostname (for mDNS resolution)
     * @param hostname Hostname (e.g., "lightwaveos.local")
     */
    void setServerHostname(const char* hostname) { _serverHostname = hostname; }

    /**
     * Get current server IP (resolved or set)
     * @return IPAddress of the v2 device
     */
    IPAddress getServerIP() const { return _serverIP; }

    /**
     * Resolve server hostname to IP (mDNS)
     * @return true if resolved, false otherwise
     */
    bool resolveHostname();

    /**
     * Start non-blocking discovery task
     * @return true if task started or already running
     */
    bool startDiscovery();

    /**
     * Get current discovery state
     */
    DiscoveryState getDiscoveryState() const;

    /**
     * Get discovered server IP (valid when SUCCESS)
     */
    IPAddress getDiscoveredIP() const;
    
    /**
     * Set optional API key for authentication
     * @param apiKey API key string (empty to disable)
     */
    void setApiKey(const String& apiKey) { _apiKey = apiKey; }

    /**
     * List saved networks
     * @param networks Output array for network entries
     * @param maxNetworks Maximum number of networks to retrieve
     * @return Number of networks retrieved, or -1 on error
     */
    int listNetworks(NetworkEntry* networks, uint8_t maxNetworks);

    /**
     * Add a network (save credentials)
     * @param ssid Network SSID
     * @param password Network password
     * @return true if successful, false otherwise
     */
    bool addNetwork(const String& ssid, const String& password);

    /**
     * Delete a saved network
     * @param ssid Network SSID to delete
     * @return true if successful, false otherwise
     */
    bool deleteNetwork(const String& ssid);

    /**
     * Connect to a network
     * @param ssid Network SSID
     * @param password Network password (empty if using saved credentials)
     * @return true if connection initiated, false otherwise
     */
    bool connectToNetwork(const String& ssid, const String& password = "");

    /**
     * Disconnect from current network
     * @return true if successful, false otherwise
     */
    bool disconnectFromNetwork();

    /**
     * Perform network scan (synchronous - v2 returns immediate results)
     * @param status Output parameter for scan results
     * @return true if scan successful, false otherwise
     */
    bool startScan(ScanStatus& status);

    /**
     * Get network status (AP/STA info)
     * @param doc Output JSON document
     * @return true if successful, false otherwise
     */
    bool getNetworkStatus(JsonDocument& doc);

private:
    IPAddress _serverIP;
    const char* _serverHostname = "lightwaveos.local";
    WiFiClient _client;
    String _apiKey;  // Optional API key (empty = disabled)
    static constexpr uint16_t HTTP_TIMEOUT_MS = 5000;
    static constexpr uint16_t HTTP_PORT = 80;

    // Discovery task state
    volatile DiscoveryState _discoveryState = DiscoveryState::IDLE;
    IPAddress _discoveryResult;
    TaskHandle_t _discoveryTaskHandle = nullptr;
    mutable SemaphoreHandle_t _discoveryMutex = nullptr;
    volatile bool _discoveryCancelRequested = false;

    /**
     * Make HTTP GET request
     * @param path API path (e.g., "/api/v1/network/networks")
     * @param response Output response structure
     * @return true if request succeeded, false otherwise
     */
    bool get(const String& path, HttpResponse& response);

    /**
     * Make HTTP POST request
     * @param path API path
     * @param body Request body (JSON)
     * @param response Output response structure
     * @return true if request succeeded, false otherwise
     */
    bool post(const String& path, const String& body, HttpResponse& response);

    /**
     * Make HTTP DELETE request
     * @param path API path (e.g., "/api/v1/network/networks/MyNetwork")
     * @param response Output response structure
     * @return true if request succeeded, false otherwise
     */
    bool del(const String& path, HttpResponse& response);

    /**
     * Connect to server
     * @return true if connected, false otherwise
     */
    bool connectToServer();

    /**
     * Parse JSON response
     * @param response HTTP response
     * @param doc Output JSON document
     * @return true if parsed successfully, false otherwise
     */
    bool parseJsonResponse(const HttpResponse& response, JsonDocument& doc);

    bool runDiscovery();
    static void discoveryTask(void* parameter);
};

#endif // ENABLE_WIFI
