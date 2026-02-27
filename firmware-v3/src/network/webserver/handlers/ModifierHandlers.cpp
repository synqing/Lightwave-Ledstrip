// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ModifierHandlers.cpp
 * @brief Effect modifier REST API handlers (stub)
 */

#include "ModifierHandlers.h"
#include "../../ApiResponse.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void ModifierHandlers::handleListModifiers(AsyncWebServerRequest* request, actors::RendererActor* renderer) {
    (void)renderer;
    sendSuccessResponse(request, [](JsonObject& data) {
        data["modifiers"] = JsonArray();
        data["count"] = 0;
        data["maxSlots"] = 8;
        data["status"] = "stub";
    });
}

void ModifierHandlers::handleAddModifier(AsyncWebServerRequest* request, uint8_t* data, size_t len, actors::RendererActor* renderer) {
    (void)data; (void)len; (void)renderer;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Modifiers not yet implemented");
}

void ModifierHandlers::handleRemoveModifier(AsyncWebServerRequest* request, uint8_t* data, size_t len, actors::RendererActor* renderer) {
    (void)data; (void)len; (void)renderer;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Modifiers not yet implemented");
}

void ModifierHandlers::handleClearModifiers(AsyncWebServerRequest* request, actors::RendererActor* renderer) {
    (void)renderer;
    sendSuccessResponse(request, [](JsonObject& data) {
        data["cleared"] = true;
        data["status"] = "stub";
    });
}

void ModifierHandlers::handleUpdateModifier(AsyncWebServerRequest* request, uint8_t* data, size_t len, actors::RendererActor* renderer) {
    (void)data; (void)len; (void)renderer;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Modifiers not yet implemented");
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
