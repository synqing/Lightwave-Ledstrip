/**
 * @file EffectPresetHandlers.cpp
 * @brief Effect preset REST API handlers (stub)
 */

#include "EffectPresetHandlers.h"
#include "../../ApiResponse.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void EffectPresetHandlers::handleList(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        data["presets"] = JsonArray();
        data["count"] = 0;
        data["status"] = "stub";
    });
}

void EffectPresetHandlers::handleSave(AsyncWebServerRequest* request, uint8_t* data, size_t len, actors::RendererActor* renderer) {
    (void)data; (void)len; (void)renderer;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Effect presets not yet implemented");
}

void EffectPresetHandlers::handleGet(AsyncWebServerRequest* request, uint8_t id) {
    (void)id;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Effect presets not yet implemented");
}

void EffectPresetHandlers::handleApply(AsyncWebServerRequest* request, uint8_t id, actors::ActorSystem& orchestrator, actors::RendererActor* renderer) {
    (void)id; (void)orchestrator; (void)renderer;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Effect presets not yet implemented");
}

void EffectPresetHandlers::handleDelete(AsyncWebServerRequest* request, uint8_t id) {
    (void)id;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Effect presets not yet implemented");
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
