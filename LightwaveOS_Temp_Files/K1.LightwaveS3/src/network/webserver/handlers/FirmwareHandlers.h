/**
 * @file FirmwareHandlers.h
 * @brief OTA firmware update and version HTTP handlers
 *
 * Provides endpoints for:
 * - GET /api/v1/firmware/version - Get current firmware version
 * - POST /api/v1/firmware/update - OTA update via v1 API
 * - POST /update - Legacy OTA update endpoint (multipart form or raw binary)
 *
 * Security: All update endpoints require X-OTA-Token header authentication.
 */

#pragma once

#include "../HttpRouteRegistry.h"
#include "../../ApiResponse.h"
#include <ESPAsyncWebServer.h>
#include <functional>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

/**
 * @brief Firmware-related HTTP handlers
 *
 * Handles OTA firmware updates via ESP32 Update library.
 * Supports both multipart/form-data and application/octet-stream uploads.
 */
class FirmwareHandlers {
public:
    /**
     * @brief Handle GET /api/v1/firmware/version
     *
     * Returns current firmware version and build information.
     * Response includes:
     * - version: Semantic version string
     * - board: Target board name
     * - sdk: ESP-IDF SDK version
     * - sketchSize: Current firmware size in bytes
     * - freeSketch: Available space for OTA
     * - flashSize: Total flash size
     * - buildDate: Compilation date
     * - buildTime: Compilation time
     */
    static void handleVersion(AsyncWebServerRequest* request);

    /**
     * @brief Handle POST /api/v1/firmware/update (v1 API endpoint)
     *
     * OTA update using v1 API response format.
     * Requires X-OTA-Token header for authentication.
     *
     * @param request HTTP request
     * @param checkOTAToken Function to validate OTA token
     */
    static void handleV1Update(AsyncWebServerRequest* request,
                                std::function<bool(AsyncWebServerRequest*)> checkOTAToken);

    /**
     * @brief Handle POST /update (legacy endpoint)
     *
     * OTA update using simple response format for curl compatibility.
     * Requires X-OTA-Token header for authentication.
     *
     * @param request HTTP request
     * @param checkOTAToken Function to validate OTA token
     */
    static void handleLegacyUpdate(AsyncWebServerRequest* request,
                                    std::function<bool(AsyncWebServerRequest*)> checkOTAToken);

    /**
     * @brief Upload handler for OTA firmware data
     *
     * Processes chunked firmware upload. Called multiple times during upload.
     * Uses ESP32 Update library for flash writing.
     *
     * @param request HTTP request
     * @param filename Uploaded filename
     * @param index Current byte offset
     * @param data Chunk data pointer
     * @param len Chunk data length
     * @param final True if this is the last chunk
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

private:
    // Track update state (static for use across handler calls)
    static bool s_updateStarted;
    static bool s_updateError;
    static String s_updateErrorMessage;
    static size_t s_updateProgress;
    static size_t s_updateTotal;
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
