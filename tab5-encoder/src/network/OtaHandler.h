// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file OtaHandler.h
 * @brief OTA firmware update handlers for Tab5.encoder
 *
 * Provides endpoints for:
 * - GET /api/v1/firmware/version - Get current firmware version
 * - POST /api/v1/firmware/update - OTA update via v1 API
 * - POST /update - Legacy OTA update endpoint (multipart form or raw binary)
 *
 * Security: All update endpoints require X-OTA-Token header authentication.
 */

#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <functional>

/**
 * @brief OTA firmware update handler
 *
 * Handles Over-The-Air firmware updates via ESP32 Update library.
 * Supports both multipart/form-data and application/octet-stream uploads.
 */
class OtaHandler {
public:
    /**
     * @brief Handle GET /api/v1/firmware/version
     *
     * Returns current firmware version and build information.
     */
    static void handleVersion(AsyncWebServerRequest* request);

    /**
     * @brief Handle POST /api/v1/firmware/update (v1 API endpoint)
     *
     * OTA update using v1 API response format.
     * Requires X-OTA-Token header for authentication.
     */
    static void handleV1Update(AsyncWebServerRequest* request);

    /**
     * @brief Handle POST /update (legacy endpoint)
     *
     * OTA update using simple response format for curl compatibility.
     * Requires X-OTA-Token header for authentication.
     */
    static void handleLegacyUpdate(AsyncWebServerRequest* request);

    /**
     * @brief Upload handler for OTA firmware data
     *
     * Processes chunked firmware upload. Called multiple times during upload.
     * Uses ESP32 Update library for flash writing.
     */
    static void handleUpload(AsyncWebServerRequest* request,
                              const String& filename,
                              size_t index,
                              uint8_t* data,
                              size_t len,
                              bool final);

    /**
     * @brief Check OTA token authentication
     *
     * Validates X-OTA-Token header against configured token.
     * Sends 401 error response if authentication fails.
     *
     * @param request HTTP request to check
     * @return true if token is valid, false otherwise
     */
    static bool checkOTAToken(AsyncWebServerRequest* request);

    /**
     * @brief Get current update progress (0-100)
     *
     * @return Progress percentage, or 0 if no update in progress
     */
    static uint8_t getProgress();

    /**
     * @brief Check if update is in progress
     *
     * @return true if update is currently in progress
     */
    static bool isUpdating();

private:
    // Track update state (static for use across handler calls)
    static bool s_updateStarted;
    static bool s_updateError;
    static String s_updateErrorMessage;
    static size_t s_updateProgress;
    static size_t s_updateTotal;

    /**
     * @brief Send JSON error response
     */
    static void sendErrorResponse(AsyncWebServerRequest* request,
                                   int code,
                                   const char* message);

    /**
     * @brief Send JSON success response
     */
    static void sendSuccessResponse(AsyncWebServerRequest* request,
                                     std::function<void(JsonObject&)> fillData);
};

