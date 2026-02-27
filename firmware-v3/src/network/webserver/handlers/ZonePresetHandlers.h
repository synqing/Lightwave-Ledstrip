// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ZonePresetHandlers.h
 * @brief Zone preset REST API handlers (stub)
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <functional>

namespace lightwaveos {
namespace actors { class ActorSystem; }
namespace zones { class ZoneComposer; }
namespace network {
namespace webserver {
namespace handlers {

class ZonePresetHandlers {
public:
    static void handleList(AsyncWebServerRequest* request);
    static void handleSave(AsyncWebServerRequest* request, uint8_t* data, size_t len, zones::ZoneComposer* composer);
    static void handleGet(AsyncWebServerRequest* request, uint8_t id);
    static void handleApply(AsyncWebServerRequest* request, uint8_t id, actors::ActorSystem& orchestrator, zones::ZoneComposer* composer, std::function<void()> broadcastFn);
    static void handleDelete(AsyncWebServerRequest* request, uint8_t id);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
