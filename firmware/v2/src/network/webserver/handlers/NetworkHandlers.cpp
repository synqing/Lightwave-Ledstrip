/**
 * @file NetworkHandlers.cpp
 * @brief Network mode/status handlers implementation
 */

#include "NetworkHandlers.h"

#include "../../WiFiManager.h"
#include "../../WiFiCredentialsStorage.h"
#include "../../../config/network_config.h"
#include "../../RequestValidator.h"
#include "../../ApiResponse.h"

#define LW_LOG_TAG "Network"
#include "utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

using namespace lightwaveos::config;

// Static scan job tracking
uint32_t NetworkHandlers::s_scanJobId = 0;
uint32_t NetworkHandlers::s_scanStartTime = 0;
bool NetworkHandlers::s_scanInProgress = false;

bool NetworkHandlers::checkOTAToken(AsyncWebServerRequest* request) {
    const char* expectedToken = NetworkConfig::OTA_UPDATE_TOKEN;

    if (!request->hasHeader("X-OTA-Token")) {
        sendErrorResponse(request, HttpStatus::UNAUTHORIZED,
                          ErrorCodes::UNAUTHORIZED, "Missing X-OTA-Token header");
        return false;
    }

    String providedToken = request->header("X-OTA-Token");
    if (providedToken != expectedToken) {
        sendErrorResponse(request, HttpStatus::UNAUTHORIZED,
                          ErrorCodes::UNAUTHORIZED, "Invalid OTA token");
        return false;
    }

    return true;
}

void NetworkHandlers::handleStatus(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["compiledForceApMode"] = NetworkConfig::FORCE_AP_MODE;
        data["runtimeForceApMode"] = WIFI_MANAGER.isForceApOnlyRuntime();

        data["state"] = WIFI_MANAGER.getStateString();

        JsonObject ap = data["ap"].to<JsonObject>();
        ap["ssid"] = NetworkConfig::AP_SSID;
        ap["ip"] = WIFI_MANAGER.getAPIP().toString();
        ap["clients"] = WiFi.softAPgetStationNum();

        JsonObject sta = data["sta"].to<JsonObject>();
        sta["connected"] = WIFI_MANAGER.isConnected();
        sta["ssid"] = WIFI_MANAGER.getSSID();
        sta["ip"] = WIFI_MANAGER.getLocalIP().toString();
        sta["rssi"] = WIFI_MANAGER.isConnected() ? WIFI_MANAGER.getRSSI() : 0;
        sta["channel"] = WIFI_MANAGER.getChannel();

        JsonObject ota = data["ota"].to<JsonObject>();
        ota["enabled"] = true;
        ota["tokenConfigured"] = (NetworkConfig::OTA_UPDATE_TOKEN && NetworkConfig::OTA_UPDATE_TOKEN[0] != '\0');
    });
}

void NetworkHandlers::handleEnableSTA(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!checkOTAToken(request)) return;

    // Defaults: stay on STA until reboot (or explicit AP-only call).
    uint32_t durationSeconds = 0;
    bool revertToApOnly = false;
    String ssidOverride = "";
    String passwordOverride = "";

    if (data && len > 0) {
        JsonDocument doc;
        VALIDATE_REQUEST_OR_RETURN(data, len, doc, RequestSchemas::NetworkStaEnable, request);
        if (doc.containsKey("durationSeconds")) {
            durationSeconds = doc["durationSeconds"].as<uint32_t>();
        }
        if (doc.containsKey("revertToApOnly")) {
            revertToApOnly = doc["revertToApOnly"].as<bool>();
        }
        if (doc.containsKey("ssid")) {
            ssidOverride = doc["ssid"].as<String>();
        }
        if (doc.containsKey("password")) {
            passwordOverride = doc["password"].as<String>();
        }

        if (!passwordOverride.isEmpty() && ssidOverride.isEmpty()) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::INVALID_VALUE, "password requires ssid");
            return;
        }
    }

    if (!ssidOverride.isEmpty()) {
        WIFI_MANAGER.setCredentials(ssidOverride, passwordOverride);
    }

    uint32_t durationMs = durationSeconds * 1000u;
    WIFI_MANAGER.requestSTAEnable(durationMs, revertToApOnly);

    sendSuccessResponse(request, [durationSeconds, revertToApOnly, ssidOverride](JsonObject& resp) {
        resp["requested"] = "sta";
        resp["durationSeconds"] = durationSeconds;
        resp["revertToApOnly"] = revertToApOnly;
        if (!ssidOverride.isEmpty()) {
            resp["ssid"] = ssidOverride;
        }
        resp["note"] = "APSTA window active (if heap allows) then STA-only. AP clients may be dropped to reclaim heap.";
    });
}

void NetworkHandlers::handleEnableAPOnly(AsyncWebServerRequest* request) {
    if (!checkOTAToken(request)) return;

    WIFI_MANAGER.requestAPOnly();

    sendSuccessResponse(request, [](JsonObject& resp) {
        resp["requested"] = "ap_only";
    });
}

// ============================================================================
// Network Management (AP-first architecture)
// ============================================================================

void NetworkHandlers::handleListNetworks(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        JsonArray networks = data["networks"].to<JsonArray>();
        
        WiFiCredentialsStorage::NetworkCredential savedNetworks[WiFiCredentialsStorage::MAX_NETWORKS];
        uint8_t count = WIFI_MANAGER.getSavedNetworks(savedNetworks, WiFiCredentialsStorage::MAX_NETWORKS);
        
        data["count"] = count;
        data["maxNetworks"] = WiFiCredentialsStorage::MAX_NETWORKS;
        
        for (uint8_t i = 0; i < count; i++) {
            JsonObject network = networks.add<JsonObject>();
            network["ssid"] = savedNetworks[i].ssid;
            // Don't send password for security (only indicate if password is set)
            network["hasPassword"] = savedNetworks[i].password.length() > 0;
        }
    });
}

void NetworkHandlers::handleAddNetwork(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!data || len == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Request body required");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data, len);
    
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Invalid JSON");
        return;
    }

    const char* ssid = doc["ssid"];
    const char* password = doc["password"];

    if (!ssid || strlen(ssid) == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "SSID required");
        return;
    }

    // Password is optional (for open networks)
    String passwordStr = password ? String(password) : String("");
    
    if (WIFI_MANAGER.addNetwork(String(ssid), passwordStr)) {
        sendSuccessResponse(request, [ssid](JsonObject& resp) {
            resp["ssid"] = ssid;
            resp["saved"] = true;
            resp["message"] = "Network saved to storage";
        });
    } else {
        sendErrorResponse(request, HttpStatus::INSUFFICIENT_STORAGE,
                          ErrorCodes::STORAGE_FULL, "Failed to save network (storage full or invalid)");
    }
}

void NetworkHandlers::handleDeleteNetwork(AsyncWebServerRequest* request, const String& ssid) {
    if (ssid.length() == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "SSID required");
        return;
    }

    if (WIFI_MANAGER.deleteSavedNetwork(ssid)) {
        sendSuccessResponse(request, [ssid](JsonObject& resp) {
            resp["ssid"] = ssid;
            resp["deleted"] = true;
            resp["message"] = "Network deleted from storage";
        });
    } else {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Network not found in storage");
    }
}

void NetworkHandlers::handleConnect(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    if (!data || len == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Request body required");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data, len);
    
    if (error) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Invalid JSON");
        return;
    }

    const char* ssid = doc["ssid"];
    const char* password = doc["password"];

    if (!ssid || strlen(ssid) == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "SSID required");
        return;
    }

    String ssidStr = String(ssid);
    String passwordStr = password ? String(password) : String("");

    // If password provided, use connectToNetwork (saves if new)
    // If no password, try connectToSavedNetwork (uses stored password)
    bool initiated = false;
    if (passwordStr.length() > 0) {
        initiated = WIFI_MANAGER.connectToNetwork(ssidStr, passwordStr);
    } else {
        initiated = WIFI_MANAGER.connectToSavedNetwork(ssidStr);
    }

    if (initiated) {
        sendSuccessResponse(request, [ssidStr](JsonObject& resp) {
            resp["ssid"] = ssidStr;
            resp["connecting"] = true;
            resp["message"] = "Connection attempt initiated (switching to STA mode)";
        });
    } else {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Network not found in saved networks. Provide password or save network first.");
    }
}

void NetworkHandlers::handleDisconnect(AsyncWebServerRequest* request) {
    WIFI_MANAGER.disconnect();
    
    sendSuccessResponse(request, [](JsonObject& resp) {
        resp["disconnected"] = true;
        resp["message"] = "STA disconnected, returning to AP-only mode";
    });
}

// ============================================================================
// Network Scanning
// ============================================================================

void NetworkHandlers::handleScanNetworks(AsyncWebServerRequest* request) {
    // Start async scan (if not already in progress)
    if (s_scanInProgress) {
        sendSuccessResponse(request, [](JsonObject& resp) {
            resp["jobId"] = s_scanJobId;
            resp["status"] = "in_progress";
            resp["message"] = "Scan already in progress";
        });
        return;
    }

    // Trigger scan
    WIFI_MANAGER.scanNetworks();

    // Generate job ID and track scan start
    s_scanJobId = millis();  // Use timestamp as job ID
    s_scanStartTime = millis();
    s_scanInProgress = true;

    sendSuccessResponse(request, [](JsonObject& resp) {
        resp["jobId"] = s_scanJobId;
        resp["status"] = "started";
        resp["message"] = "Network scan initiated (check status endpoint for results)";
    });
}

void NetworkHandlers::handleScanStatus(AsyncWebServerRequest* request) {
    // Check scan completion by comparing lastScanTime with scan start time
    uint32_t lastScanTime = WIFI_MANAGER.getLastScanTime();
    uint32_t elapsed = millis() - s_scanStartTime;
    
    // Scan is complete if lastScanTime was updated after scan started
    // OR if scan was started more than 10 seconds ago (timeout)
    bool scanComplete = (lastScanTime > s_scanStartTime && lastScanTime < millis()) || 
                        (elapsed > 10000);
    
    if (s_scanInProgress && !scanComplete && elapsed < 10000) {
        // Scan still in progress
        sendSuccessResponse(request, [elapsed](JsonObject& resp) {
            resp["jobId"] = s_scanJobId;
            resp["status"] = "in_progress";
            resp["elapsedMs"] = elapsed;
            resp["message"] = "Scan in progress, check again in a few seconds";
        });
        return;
    }
    
    // Scan complete or timed out - mark as complete
    if (s_scanInProgress) {
        s_scanInProgress = false;
    }
    
    if (elapsed > 10000 && lastScanTime < s_scanStartTime) {
        // Scan timeout (no results yet)
        sendErrorResponse(request, HttpStatus::REQUEST_TIMEOUT,
                          ErrorCodes::BUSY, "Scan timeout - no results available");
        return;
    }

    // Scan complete - return results
    sendSuccessResponse(request, [lastScanTime, elapsed](JsonObject& data) {
        data["jobId"] = s_scanJobId;
        data["status"] = "complete";
        
        const auto& scanResults = WIFI_MANAGER.getScanResults();
        
        data["lastScanTime"] = lastScanTime;
        data["age"] = millis() - lastScanTime;
        data["scanDurationMs"] = elapsed;
        
        JsonArray networks = data["networks"].to<JsonArray>();
        
        for (const auto& result : scanResults) {
            JsonObject network = networks.add<JsonObject>();
            network["ssid"] = result.ssid;
            network["rssi"] = result.rssi;
            network["channel"] = result.channel;
            network["encryption"] = (uint8_t)result.encryption;
            
            // Format BSSID as hex string
            char bssidStr[18];
            snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                     result.bssid[0], result.bssid[1], result.bssid[2],
                     result.bssid[3], result.bssid[4], result.bssid[5]);
            network["bssid"] = bssidStr;
        }
        
        data["count"] = scanResults.size();
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

