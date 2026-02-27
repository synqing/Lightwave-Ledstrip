// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ShowHandlers.cpp
 * @brief Show management REST API handlers (stub)
 */

#include "ShowHandlers.h"
#include "../../ApiResponse.h"

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void ShowHandlers::handleCurrent(AsyncWebServerRequest* request, actors::ActorSystem& orchestrator) {
    (void)orchestrator;
    sendSuccessResponse(request, [](JsonObject& data) {
        data["currentShow"] = nullptr;
        data["status"] = "stub";
    });
}

void ShowHandlers::handleGet(AsyncWebServerRequest* request, const String& showId, const String& format, actors::ActorSystem& orchestrator) {
    (void)showId; (void)format; (void)orchestrator;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Shows not yet implemented");
}

void ShowHandlers::handleList(AsyncWebServerRequest* request, actors::ActorSystem& orchestrator) {
    (void)orchestrator;
    sendSuccessResponse(request, [](JsonObject& data) {
        data["shows"] = JsonArray();
        data["count"] = 0;
        data["status"] = "stub";
    });
}

void ShowHandlers::handleCreate(AsyncWebServerRequest* request, uint8_t* data, size_t len, actors::ActorSystem& orchestrator) {
    (void)data; (void)len; (void)orchestrator;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Shows not yet implemented");
}

void ShowHandlers::handleUpdate(AsyncWebServerRequest* request, const String& showId, uint8_t* data, size_t len, actors::ActorSystem& orchestrator) {
    (void)showId; (void)data; (void)len; (void)orchestrator;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Shows not yet implemented");
}

void ShowHandlers::handleDelete(AsyncWebServerRequest* request, const String& showId, actors::ActorSystem& orchestrator) {
    (void)showId; (void)orchestrator;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Shows not yet implemented");
}

void ShowHandlers::handleControl(AsyncWebServerRequest* request, uint8_t* data, size_t len, actors::ActorSystem& orchestrator) {
    (void)data; (void)len; (void)orchestrator;
    sendErrorResponse(request, HttpStatus::NOT_FOUND, "NOT_IMPLEMENTED", "Shows not yet implemented");
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
