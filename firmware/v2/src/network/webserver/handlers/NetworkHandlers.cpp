/**
 * @file NetworkHandlers.cpp
 * @brief Network management HTTP handlers implementation
 *
 * LightwaveOS v2 - Network Subsystem
 */

#include "NetworkHandlers.h"
#include "../../WiFiManager.h"
#include "../../WiFiCredentialManager.h"
#include "../../RequestValidator.h"
#include "../../ApiResponse.h"
#include <WiFi.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

// ============================================================================
// Route Registration
// ============================================================================

void NetworkHandlers::registerRoutes(HttpRouteRegistry& registry) {
    // Routes are registered in V1ApiRoutes.cpp with rate limiting wrappers
    (void)registry;
}

// ============================================================================
// Status & Scanning
// ============================================================================

void NetworkHandlers::handleStatus(AsyncWebServerRequest* request) {
    WiFiManager& wm = WIFI_MANAGER;

    sendSuccessResponse(request, [&wm](JsonObject& data) {
        bool connected = wm.isConnected();
        bool apMode = wm.isAPMode();

        data["connected"] = connected;
        data["state"] = wm.getStateString();
        data["apMode"] = apMode;

        if (connected) {
            data["ssid"] = wm.getSSID();
            data["ip"] = wm.getLocalIP().toString();
            data["rssi"] = wm.getRSSI();
            data["channel"] = wm.getChannel();
        } else if (apMode) {
            data["apIP"] = wm.getAPIP().toString();
        }

        // Connection statistics
        JsonObject stats = data["stats"].to<JsonObject>();
        stats["connectionAttempts"] = wm.getConnectionAttempts();
        stats["successfulConnections"] = wm.getSuccessfulConnections();
        stats["uptimeSeconds"] = wm.getUptimeSeconds();
    });
}

void NetworkHandlers::handleScan(AsyncWebServerRequest* request) {
    WiFiManager& wm = WIFI_MANAGER;

    // Trigger a fresh scan
    wm.scanNetworks();

    // Get cached scan results
    // Note: WiFi scan takes 2-5 seconds. In practice, you may want to:
    // 1. Return cached results immediately
    // 2. Start async scan and return 202 with scan ID
    // For simplicity, we return cached results from last scan
    const auto& results = wm.getScanResults();

    sendSuccessResponse(request, [&results, &wm](JsonObject& data) {
        JsonArray networks = data["networks"].to<JsonArray>();

        for (const auto& net : results) {
            JsonObject entry = networks.add<JsonObject>();
            entry["ssid"] = net.ssid;
            entry["rssi"] = net.rssi;
            entry["channel"] = net.channel;
            entry["encryption"] = authModeToString(static_cast<uint8_t>(net.encryption));

            // Format BSSID as MAC address string
            char bssid[18];
            snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
                     net.bssid[0], net.bssid[1], net.bssid[2],
                     net.bssid[3], net.bssid[4], net.bssid[5]);
            entry["bssid"] = bssid;
        }

        data["count"] = results.size();
        data["lastScanTime"] = wm.getLastScanTime();
    });
}

// ============================================================================
// Connection Management
// ============================================================================

void NetworkHandlers::handleConnect(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;

    // Validate request using existing schema
    auto result = RequestValidator::parseAndValidate(data, len, doc,
                                                      RequestSchemas::NetworkConnect);
    if (!result.valid) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          result.errorCode, result.errorMessage, result.fieldName);
        return;
    }

    const char* ssid = doc["ssid"].as<const char*>();
    const char* password = doc["password"] | "";  // Empty string for open networks
    bool saveNetwork = doc["save"] | false;

    // Input validation
    if (strlen(ssid) == 0 || strlen(ssid) > 32) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "SSID must be 1-32 characters");
        return;
    }

    // Password validation for secured networks (WPA2 min 8 chars)
    // Allow empty password for open networks
    size_t passLen = strlen(password);
    if (passLen > 0 && passLen < 8) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Password must be at least 8 characters");
        return;
    }

    // Save to credential manager if requested
    if (saveNetwork && passLen > 0) {
        WIFI_CREDENTIALS.addNetwork(ssid, password);
    }

    // Initiate connection via WiFiManager
    WIFI_MANAGER.setCredentials(ssid, password);
    WIFI_MANAGER.reconnect();

    // Return 202 Accepted - connection happens asynchronously
    sendSuccessResponse(request, [ssid](JsonObject& data) {
        data["message"] = "Connection attempt initiated";
        data["ssid"] = ssid;
    }, HttpStatus::ACCEPTED);
}

void NetworkHandlers::handleDisconnect(AsyncWebServerRequest* request) {
    WIFI_MANAGER.disconnect();

    sendSuccessResponse(request, [](JsonObject& data) {
        data["message"] = "Disconnected";
    });
}

// ============================================================================
// Saved Networks
// ============================================================================

void NetworkHandlers::handleSavedList(AsyncWebServerRequest* request) {
    auto ssids = WIFI_CREDENTIALS.getSavedSSIDs();

    sendSuccessResponse(request, [&ssids](JsonObject& data) {
        JsonArray networks = data["networks"].to<JsonArray>();
        for (const auto& ssid : ssids) {
            networks.add(ssid);
        }
        data["count"] = ssids.size();
        data["maxNetworks"] = MAX_SAVED_NETWORKS;
    });
}

void NetworkHandlers::handleSavedAdd(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    JsonDocument doc;

    // Use centralized schema from RequestSchemas::NetworkSave
    auto result = RequestValidator::parseAndValidate(data, len, doc,
                                                      RequestSchemas::NetworkSave);
    if (!result.valid) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          result.errorCode, result.errorMessage, result.fieldName);
        return;
    }

    const char* ssid = doc["ssid"].as<const char*>();
    const char* password = doc["password"].as<const char*>();

    // Check if we have room
    if (WIFI_CREDENTIALS.getNetworkCount() >= MAX_SAVED_NETWORKS &&
        !WIFI_CREDENTIALS.hasNetwork(ssid)) {
        sendErrorResponse(request, HttpStatus::CONFLICT,
                          "NETWORK_LIMIT_REACHED",
                          "Maximum saved networks reached (8). Remove a network first.");
        return;
    }

    // Add/update network
    if (!WIFI_CREDENTIALS.addNetwork(ssid, password)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_SERVER_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to save network");
        return;
    }

    sendSuccessResponse(request, [ssid](JsonObject& data) {
        data["message"] = "Network saved";
        data["ssid"] = ssid;
    }, HttpStatus::CREATED);
}

void NetworkHandlers::handleSavedDelete(AsyncWebServerRequest* request) {
    // Extract SSID from URL path: /api/v1/network/saved/{ssid}
    String url = request->url();

    // Find the SSID after /saved/
    int savedIdx = url.indexOf("/saved/");
    if (savedIdx < 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "SSID required in path");
        return;
    }

    String ssidEncoded = url.substring(savedIdx + 7);  // Length of "/saved/"
    if (ssidEncoded.isEmpty()) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD, "SSID required in path");
        return;
    }

    // URL-decode the SSID
    String ssid = "";
    for (size_t i = 0; i < ssidEncoded.length(); i++) {
        char c = ssidEncoded[i];
        if (c == '%' && i + 2 < ssidEncoded.length()) {
            // Decode percent-encoded character
            char hex[3] = { ssidEncoded[i+1], ssidEncoded[i+2], '\0' };
            ssid += (char)strtol(hex, nullptr, 16);
            i += 2;
        } else if (c == '+') {
            ssid += ' ';
        } else {
            ssid += c;
        }
    }

    // Attempt to remove
    if (!WIFI_CREDENTIALS.hasNetwork(ssid.c_str())) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Network not found");
        return;
    }

    if (!WIFI_CREDENTIALS.removeNetwork(ssid.c_str())) {
        sendErrorResponse(request, HttpStatus::INTERNAL_SERVER_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to remove network");
        return;
    }

    sendSuccessResponse(request, [&ssid](JsonObject& data) {
        data["message"] = "Network removed";
        data["ssid"] = ssid;
    });
}

// ============================================================================
// Utility
// ============================================================================

const char* NetworkHandlers::authModeToString(uint8_t authMode) {
    switch (static_cast<wifi_auth_mode_t>(authMode)) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
        default:                        return "UNKNOWN";
    }
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
