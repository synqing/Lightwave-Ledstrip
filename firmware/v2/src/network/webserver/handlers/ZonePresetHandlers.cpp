/**
 * @file ZonePresetHandlers.cpp
 * @brief Zone preset REST API handlers (stub)
 */

#include "ZonePresetHandlers.h"
#include "../../ApiResponse.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void ZonePresetHandlers::handleList(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["presets"] = JsonArray();
        data["count"] = 0;
        data["status"] = "stub";
    });
}

void ZonePresetHandlers::handleSave(AsyncWebServerRequest* request, uint8_t* data, size_t len, zones::ZoneComposer* composer) {
    (void)data; (void)len; (void)composer;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Zone presets not yet implemented");
}

void ZonePresetHandlers::handleGet(AsyncWebServerRequest* request, uint8_t id) {
    (void)id;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Zone presets not yet implemented");
}

void ZonePresetHandlers::handleApply(AsyncWebServerRequest* request, uint8_t id, actors::ActorSystem& orchestrator, zones::ZoneComposer* composer, std::function<void()> broadcastFn) {
    (void)id; (void)orchestrator; (void)composer; (void)broadcastFn;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Zone presets not yet implemented");
}

void ZonePresetHandlers::handleDelete(AsyncWebServerRequest* request, uint8_t id) {
    (void)id;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Zone presets not yet implemented");
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
