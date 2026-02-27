// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file FirmwareHandlers.h
 * @brief Firmware/OTA update REST API handlers
 *
 * Provides REST endpoints for firmware version query and OTA firmware upload.
 * Three upload routes share similar chunk handler patterns:
 *   - POST /api/v1/firmware/update       (V1 JSON response, U_FLASH)
 *   - POST /api/v1/firmware/filesystem   (V1 JSON response, U_SPIFFS/LittleFS)
 *   - POST /update                       (Legacy plain-text response, U_FLASH)
 *
 * Authentication: X-OTA-Token header validated against OtaTokenManager (per-device NVS token).
 *
 * Usage (curl):
 *   # Firmware update
 *   curl -X POST http://lightwaveos.local/api/v1/firmware/update \
 *        -H "X-OTA-Token: LW-OTA-2024-SecureUpdate" \
 *        -F "firmware=@firmware.bin"
 *
 *   # Filesystem update (LittleFS image)
 *   curl -X POST http://lightwaveos.local/api/v1/firmware/filesystem \
 *        -H "X-OTA-Token: LW-OTA-2024-SecureUpdate" \
 *        -F "filesystem=@littlefs.bin"
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <functional>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class FirmwareHandlers {
public:
    /// GET /api/v1/firmware/version - returns firmware version and flash info
    static void handleVersion(AsyncWebServerRequest* request);

    /// POST /api/v1/firmware/update - request completion handler (V1 JSON response)
    static void handleV1Update(AsyncWebServerRequest* request, std::function<bool(AsyncWebServerRequest*)> checkToken);

    /// POST /update - request completion handler (legacy plain-text response)
    static void handleLegacyUpdate(AsyncWebServerRequest* request, std::function<bool(AsyncWebServerRequest*)> checkToken);

    /// Multipart upload chunk handler - shared by both V1 and legacy firmware routes
    static void handleUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final);

    /// POST /api/v1/firmware/filesystem - request completion handler for filesystem OTA
    static void handleV1FsUpdate(AsyncWebServerRequest* request, std::function<bool(AsyncWebServerRequest*)> checkToken);

    /// Multipart upload chunk handler for filesystem OTA (U_SPIFFS partition)
    static void handleFsUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final);

    /// Validate X-OTA-Token header using constant-time comparison
    static bool checkOTAToken(AsyncWebServerRequest* request);

    /**
     * @brief Check if a REST OTA upload is currently in progress
     *
     * Used by WiFiManager to avoid STA retry during OTA uploads,
     * which would tear down the AP and interrupt the transfer.
     * Covers both firmware (U_FLASH) and filesystem (U_SPIFFS) uploads.
     *
     * @return true if a REST firmware or filesystem upload is actively writing to flash
     */
    static bool isRestOtaInProgress();

    /// GET /api/v1/device/ota-token - returns the current per-device OTA token
    static void handleGetOtaToken(AsyncWebServerRequest* request);

    /// POST /api/v1/device/ota-token - regenerate or manually set a new OTA token
    static void handleSetOtaToken(AsyncWebServerRequest* request, uint8_t* data, size_t len);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
