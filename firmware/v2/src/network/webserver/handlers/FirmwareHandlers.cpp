/**
 * @file FirmwareHandlers.cpp
 * @brief Firmware/OTA update REST API handlers (stub)
 */

#include "FirmwareHandlers.h"
#include "../../ApiResponse.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "2.0.0-dev"
#endif

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void FirmwareHandlers::handleVersion(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["version"] = FIRMWARE_VERSION;
        data["platform"] = "ESP32-S3";
        data["status"] = "running";
    });
}

void FirmwareHandlers::handleV1Update(AsyncWebServerRequest* request, std::function<bool(AsyncWebServerRequest*)> checkToken) {
    (void)checkToken;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "OTA update not yet implemented");
}

void FirmwareHandlers::handleLegacyUpdate(AsyncWebServerRequest* request, std::function<bool(AsyncWebServerRequest*)> checkToken) {
    (void)checkToken;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "OTA update not yet implemented");
}

void FirmwareHandlers::handleUpload(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
    (void)request; (void)filename; (void)index; (void)data; (void)len; (void)final;
    // Upload handler - nothing to do in stub
}

bool FirmwareHandlers::checkOTAToken(AsyncWebServerRequest* request) {
    (void)request;
    return false;  // Always reject OTA in stub
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
