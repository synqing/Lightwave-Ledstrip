/**
 * @file ColorCorrectionHandlers.cpp
 * @brief Color correction REST API handlers (stub)
 */

#include "ColorCorrectionHandlers.h"
#include "../../ApiResponse.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void ColorCorrectionHandlers::handleGetConfig(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["mode"] = 0;
        data["modeName"] = "OFF";
        data["status"] = "stub";
    });
}

void ColorCorrectionHandlers::handleSetMode(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    (void)data; (void)len;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Color correction not yet implemented");
}

void ColorCorrectionHandlers::handleSetConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    (void)data; (void)len;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Color correction not yet implemented");
}

void ColorCorrectionHandlers::handleSave(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["saved"] = true;
        data["status"] = "stub";
    });
}

void ColorCorrectionHandlers::handleGetPresets(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["presets"] = JsonArray();
        data["count"] = 0;
        data["status"] = "stub";
    });
}

void ColorCorrectionHandlers::handleSetPreset(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    (void)data; (void)len;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Color correction not yet implemented");
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
