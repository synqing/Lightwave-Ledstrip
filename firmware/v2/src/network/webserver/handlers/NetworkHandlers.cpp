/**
 * @file NetworkHandlers.cpp
 * @brief Network mode/status handlers implementation
 */

#include "NetworkHandlers.h"

#include "../../WiFiManager.h"
#include "../../../config/network_config.h"
#include "../../RequestValidator.h"

#define LW_LOG_TAG "Network"
#include "utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

using namespace lightwaveos::config;

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
        resp["note"] = "Switching network mode may temporarily drop clients while STA connects.";
    });
}

void NetworkHandlers::handleEnableAPOnly(AsyncWebServerRequest* request) {
    if (!checkOTAToken(request)) return;

    WIFI_MANAGER.requestAPOnly();

    sendSuccessResponse(request, [](JsonObject& resp) {
        resp["requested"] = "ap_only";
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

