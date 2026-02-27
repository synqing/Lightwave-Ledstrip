// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ShowHandlers.h
 * @brief Show management REST API handlers (stub)
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace lightwaveos {
namespace actors { class ActorSystem; }
namespace network {
namespace webserver {
namespace handlers {

class ShowHandlers {
public:
    static void handleCurrent(AsyncWebServerRequest* request, actors::ActorSystem& orchestrator);
    static void handleGet(AsyncWebServerRequest* request, const String& showId, const String& format, actors::ActorSystem& orchestrator);
    static void handleList(AsyncWebServerRequest* request, actors::ActorSystem& orchestrator);
    static void handleCreate(AsyncWebServerRequest* request, uint8_t* data, size_t len, actors::ActorSystem& orchestrator);
    static void handleUpdate(AsyncWebServerRequest* request, const String& showId, uint8_t* data, size_t len, actors::ActorSystem& orchestrator);
    static void handleDelete(AsyncWebServerRequest* request, const String& showId, actors::ActorSystem& orchestrator);
    static void handleControl(AsyncWebServerRequest* request, uint8_t* data, size_t len, actors::ActorSystem& orchestrator);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
