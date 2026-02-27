/**
 * @file PresetHandlers.cpp
 * @brief Preset management REST API handlers (stub)
 */

#include "PresetHandlers.h"
#include "../../ApiResponse.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void PresetHandlers::handleList(AsyncWebServerRequest* request, persistence::PresetManager* mgr) {
    (void)mgr;
    sendSuccessResponse(request, [](JsonObject& data) {
        data["presets"] = JsonArray();
        data["count"] = 0;
        data["status"] = "stub";
    });
}

void PresetHandlers::handleGet(AsyncWebServerRequest* request, persistence::PresetManager* mgr) {
    (void)mgr;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Presets not yet implemented");
}

void PresetHandlers::handleSave(AsyncWebServerRequest* request, uint8_t* data, size_t len, persistence::PresetManager* mgr) {
    (void)data; (void)len; (void)mgr;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Presets not yet implemented");
}

void PresetHandlers::handleUpdate(AsyncWebServerRequest* request, uint8_t* data, size_t len, persistence::PresetManager* mgr) {
    (void)data; (void)len; (void)mgr;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Presets not yet implemented");
}

void PresetHandlers::handleDelete(AsyncWebServerRequest* request, persistence::PresetManager* mgr) {
    (void)mgr;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Presets not yet implemented");
}

void PresetHandlers::handleRename(AsyncWebServerRequest* request, uint8_t* data, size_t len, persistence::PresetManager* mgr) {
    (void)data; (void)len; (void)mgr;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Presets not yet implemented");
}

void PresetHandlers::handleLoad(AsyncWebServerRequest* request, zones::ZoneComposer* composer, persistence::ZoneConfigManager* zoneConfigMgr, persistence::PresetManager* presetMgr, std::function<void()> broadcastFn) {
    (void)composer; (void)zoneConfigMgr; (void)presetMgr; (void)broadcastFn;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Presets not yet implemented");
}

void PresetHandlers::handleSaveCurrent(AsyncWebServerRequest* request, uint8_t* data, size_t len, zones::ZoneComposer* composer, persistence::ZoneConfigManager* zoneConfigMgr, persistence::PresetManager* presetMgr) {
    (void)data; (void)len; (void)composer; (void)zoneConfigMgr; (void)presetMgr;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Presets not yet implemented");
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
