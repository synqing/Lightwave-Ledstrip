// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file OtaHandler.cpp
 * @brief OTA firmware update handlers implementation
 */

#include "OtaHandler.h"
#include "../config/network_config.h"
#include <ArduinoJson.h>
#include <Update.h>

// Static member initialization
bool OtaHandler::s_updateStarted = false;
bool OtaHandler::s_updateError = false;
String OtaHandler::s_updateErrorMessage = "";
size_t OtaHandler::s_updateProgress = 0;
size_t OtaHandler::s_updateTotal = 0;

// Firmware version constants
static constexpr const char* FIRMWARE_VERSION = "1.0.0";
static constexpr const char* BOARD_NAME = "M5Stack-Tab5-ESP32-P4";

void OtaHandler::handleVersion(AsyncWebServerRequest* request) {
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

bool OtaHandler::checkOTAToken(AsyncWebServerRequest* request) {
    const char* expectedToken = OTA_UPDATE_TOKEN;

    // Check for X-OTA-Token header
    if (!request->hasHeader("X-OTA-Token")) {
        Serial.printf("[OTA] Request missing X-OTA-Token header from %s\n",
                      request->client()->remoteIP().toString().c_str());
        sendErrorResponse(request, 401, "Missing X-OTA-Token header");
        return false;
    }

    // Validate token
    String providedToken = request->header("X-OTA-Token");
    if (providedToken != expectedToken) {
        Serial.printf("[OTA] Request with invalid token from %s\n",
                      request->client()->remoteIP().toString().c_str());
        sendErrorResponse(request, 401, "Invalid OTA token");
        return false;
    }

    return true;
}

void OtaHandler::handleV1Update(AsyncWebServerRequest* request) {
    // This is called after upload completes
    if (s_updateError) {
        Serial.printf("[OTA] Update failed: %s\n", s_updateErrorMessage.c_str());
        sendErrorResponse(request, 500, s_updateErrorMessage.c_str());
        // Reset state for next attempt
        s_updateError = false;
        s_updateErrorMessage = "";
        s_updateStarted = false;
        return;
    }

    if (!s_updateStarted) {
        // No upload was processed
        sendErrorResponse(request, 400, "No firmware data received");
        return;
    }

    // Success - device will reboot
    Serial.println("[OTA] Update successful, rebooting...");

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

void OtaHandler::handleLegacyUpdate(AsyncWebServerRequest* request) {
    // This is called after upload completes
    if (s_updateError) {
        Serial.printf("[OTA] Update failed: %s\n", s_updateErrorMessage.c_str());
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
    Serial.println("[OTA] Update successful, rebooting...");
    request->send(200, "text/plain", "Update successful. Rebooting...");

    // Reset state
    s_updateStarted = false;
    s_updateProgress = 0;
    s_updateTotal = 0;

    // Delay to allow response to be sent, then reboot
    delay(500);
    ESP.restart();
}

void OtaHandler::handleUpload(AsyncWebServerRequest* request,
                               const String& filename,
                               size_t index,
                               uint8_t* data,
                               size_t len,
                               bool final) {
    // On first chunk (index == 0), initialize update
    if (index == 0) {
        Serial.printf("[OTA] Upload starting: %s\n", filename.c_str());

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
        Serial.printf("[OTA] Firmware size: %u bytes\n", s_updateTotal);

        // Check available space
        size_t freeSpace = ESP.getFreeSketchSpace();
        if (s_updateTotal > freeSpace) {
            s_updateError = true;
            s_updateErrorMessage = "Firmware too large. Available: " + String(freeSpace) + " bytes";
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            return;
        }

        // Begin update
        if (!Update.begin(s_updateTotal, U_FLASH)) {
            s_updateError = true;
            s_updateErrorMessage = "Update.begin() failed: " + String(Update.errorString());
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            return;
        }

        Serial.printf("[OTA] Update started, expecting %u bytes\n", s_updateTotal);
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
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            Update.abort();
            return;
        }
        s_updateProgress += len;

        // Log progress every ~10%
        if (s_updateTotal > 0) {
            static uint8_t lastPercent = 0;
            uint8_t percent = (s_updateProgress * 100) / s_updateTotal;
            if (percent / 10 > lastPercent / 10) {
                Serial.printf("[OTA] Progress: %u%%\n", percent);
                lastPercent = percent;
            }
        }
    }

    // On final chunk, complete update
    if (final) {
        Serial.println("[OTA] Upload complete, finalizing...");

        if (!Update.end(true)) {
            s_updateError = true;
            s_updateErrorMessage = "Update.end() failed: " + String(Update.errorString());
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            return;
        }

        if (!Update.isFinished()) {
            s_updateError = true;
            s_updateErrorMessage = "Update not finished properly";
            Serial.printf("[OTA] ERROR: %s\n", s_updateErrorMessage.c_str());
            return;
        }

        Serial.println("[OTA] Upload finalized successfully");
    }
}

uint8_t OtaHandler::getProgress() {
    if (!s_updateStarted || s_updateTotal == 0) {
        return 0;
    }
    return (s_updateProgress * 100) / s_updateTotal;
}

bool OtaHandler::isUpdating() {
    return s_updateStarted && !s_updateError;
}

void OtaHandler::sendErrorResponse(AsyncWebServerRequest* request,
                                    int code,
                                    const char* message) {
    JsonDocument doc;
    doc["success"] = false;
    doc["error"] = message;
    doc["code"] = code;
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    request->send(code, "application/json", output);
}

void OtaHandler::sendSuccessResponse(AsyncWebServerRequest* request,
                                      std::function<void(JsonObject&)> fillData) {
    JsonDocument doc;
    doc["success"] = true;
    JsonObject data = doc["data"].to<JsonObject>();
    fillData(data);
    doc["timestamp"] = millis();
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

