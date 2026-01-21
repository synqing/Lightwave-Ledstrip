/**
 * @file FirmwareHandlers.cpp
 * @brief OTA firmware update handlers implementation
 */

#include "FirmwareHandlers.h"
#include "config/network_config.h"
#include <Update.h>
#include <Arduino.h>

#define LW_LOG_TAG "Firmware"
#include "utils/Log.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

// Static member initialization
bool FirmwareHandlers::s_updateStarted = false;
bool FirmwareHandlers::s_updateError = false;
String FirmwareHandlers::s_updateErrorMessage = "";
size_t FirmwareHandlers::s_updateProgress = 0;
size_t FirmwareHandlers::s_updateTotal = 0;

// Firmware version constants
static constexpr const char* FIRMWARE_VERSION = "2.0.0";
static constexpr const char* BOARD_NAME = "ESP32-S3-DevKitC-1";

void FirmwareHandlers::handleVersion(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["version"] = FIRMWARE_VERSION;
        data["board"] = BOARD_NAME;
        data["sdk"] = ESP.getSdkVersion();
        data["sketchSize"] = ESP.getSketchSize();
        data["freeSketch"] = ESP.getFreeSketchSpace();
        data["flashSize"] = ESP.getFlashChipSize();
        data["buildDate"] = __DATE__;
        data["buildTime"] = __TIME__;
        data["chipModel"] = ESP.getChipModel();
        data["chipRevision"] = ESP.getChipRevision();
        data["cpuFreq"] = ESP.getCpuFreqMHz();

        // Calculate OTA capacity
        size_t maxOtaSize = ESP.getFreeSketchSpace();
        data["maxOtaSize"] = maxOtaSize;
        data["otaAvailable"] = maxOtaSize > 0;
    });
}

bool FirmwareHandlers::checkOTAToken(AsyncWebServerRequest* request) {
    const char* expectedToken = config::NetworkConfig::OTA_UPDATE_TOKEN;

    // Check for X-OTA-Token header
    if (!request->hasHeader("X-OTA-Token")) {
        LW_LOGW("OTA request missing X-OTA-Token header from %s",
                request->client()->remoteIP().toString().c_str());
        sendErrorResponse(request, HttpStatus::UNAUTHORIZED,
                          ErrorCodes::UNAUTHORIZED, "Missing X-OTA-Token header");
        return false;
    }

    // Validate token
    String providedToken = request->header("X-OTA-Token");
    if (providedToken != expectedToken) {
        LW_LOGW("OTA request with invalid token from %s",
                request->client()->remoteIP().toString().c_str());
        sendErrorResponse(request, HttpStatus::UNAUTHORIZED,
                          ErrorCodes::UNAUTHORIZED, "Invalid OTA token");
        return false;
    }

    return true;
}

void FirmwareHandlers::handleV1Update(AsyncWebServerRequest* request,
                                       std::function<bool(AsyncWebServerRequest*)> checkOTAToken) {
    // This is called after upload completes
    if (s_updateError) {
        LW_LOGE("OTA update failed: %s", s_updateErrorMessage.c_str());
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, s_updateErrorMessage.c_str());
        // Reset state for next attempt
        s_updateError = false;
        s_updateErrorMessage = "";
        s_updateStarted = false;
        return;
    }

    if (!s_updateStarted) {
        // No upload was processed
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "No firmware data received");
        return;
    }

    // Success - device will reboot
    LW_LOGI("OTA update successful, rebooting...");

    sendSuccessResponse(request, [](JsonObject& data) {
        data["message"] = "Firmware update successful. Device is rebooting.";
        data["rebooting"] = true;
    });

    // Reset state
    s_updateStarted = false;
    s_updateProgress = 0;
    s_updateTotal = 0;

    // Delay to allow response to be sent, then reboot
    delay(500);
    ESP.restart();
}

void FirmwareHandlers::handleLegacyUpdate(AsyncWebServerRequest* request,
                                           std::function<bool(AsyncWebServerRequest*)> checkOTAToken) {
    // This is called after upload completes
    if (s_updateError) {
        LW_LOGE("OTA update failed: %s", s_updateErrorMessage.c_str());
        request->send(500, "text/plain", "Update failed: " + s_updateErrorMessage);
        // Reset state for next attempt
        s_updateError = false;
        s_updateErrorMessage = "";
        s_updateStarted = false;
        return;
    }

    if (!s_updateStarted) {
        // No upload was processed
        request->send(400, "text/plain", "No firmware data received");
        return;
    }

    // Success - device will reboot
    LW_LOGI("OTA update successful, rebooting...");
    request->send(200, "text/plain", "Update successful. Rebooting...");

    // Reset state
    s_updateStarted = false;
    s_updateProgress = 0;
    s_updateTotal = 0;

    // Delay to allow response to be sent, then reboot
    delay(500);
    ESP.restart();
}

void FirmwareHandlers::handleUpload(AsyncWebServerRequest* request,
                                     const String& filename,
                                     size_t index,
                                     uint8_t* data,
                                     size_t len,
                                     bool final) {
    // On first chunk (index == 0), initialize update
    if (index == 0) {
        LW_LOGI("OTA upload starting: %s", filename.c_str());

        // Validate OTA token before starting update
        if (!checkOTAToken(request)) {
            s_updateError = true;
            s_updateErrorMessage = "Unauthorized";
            return;
        }

        // Reset state
        s_updateError = false;
        s_updateErrorMessage = "";
        s_updateStarted = true;
        s_updateProgress = 0;

        // Get content length
        s_updateTotal = request->contentLength();
        LW_LOGI("Firmware size: %u bytes", s_updateTotal);

        // Check available space
        size_t freeSpace = ESP.getFreeSketchSpace();
        if (s_updateTotal > freeSpace) {
            s_updateError = true;
            s_updateErrorMessage = "Firmware too large. Available: " + String(freeSpace) + " bytes";
            LW_LOGE("%s", s_updateErrorMessage.c_str());
            return;
        }

        // Begin update
        if (!Update.begin(s_updateTotal, U_FLASH)) {
            s_updateError = true;
            s_updateErrorMessage = "Update.begin() failed: " + String(Update.errorString());
            LW_LOGE("%s", s_updateErrorMessage.c_str());
            return;
        }

        LW_LOGI("Update started, expecting %u bytes", s_updateTotal);
    }

    // Skip processing if we've already encountered an error
    if (s_updateError) {
        return;
    }

    // Write chunk to flash
    if (len > 0) {
        size_t written = Update.write(data, len);
        if (written != len) {
            s_updateError = true;
            s_updateErrorMessage = "Flash write failed at offset " + String(index);
            LW_LOGE("%s", s_updateErrorMessage.c_str());
            Update.abort();
            return;
        }
        s_updateProgress += len;

        // Log progress every ~10%
        if (s_updateTotal > 0) {
            static uint8_t lastPercent = 0;
            uint8_t percent = (s_updateProgress * 100) / s_updateTotal;
            if (percent / 10 > lastPercent / 10) {
                LW_LOGI("OTA progress: %u%%", percent);
                lastPercent = percent;
            }
        }
    }

    // On final chunk, complete update
    if (final) {
        LW_LOGI("OTA upload complete, finalizing...");

        if (!Update.end(true)) {
            s_updateError = true;
            s_updateErrorMessage = "Update.end() failed: " + String(Update.errorString());
            LW_LOGE("%s", s_updateErrorMessage.c_str());
            return;
        }

        if (!Update.isFinished()) {
            s_updateError = true;
            s_updateErrorMessage = "Update not finished properly";
            LW_LOGE("%s", s_updateErrorMessage.c_str());
            return;
        }

        LW_LOGI("OTA upload finalized successfully");
    }
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
