// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file ParameterHandlers.h
 * @brief Parameter-related HTTP handlers
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>
#include "../../WebServer.h"  // For CachedRendererState

namespace lightwaveos {
namespace actors {
class ActorSystem;
class RendererActor;
}
}

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class ParameterHandlers {
public:
    static void handleGet(AsyncWebServerRequest* request,
                           const lightwaveos::network::WebServer::CachedRendererState& cachedState);
    
    static void handleSet(AsyncWebServerRequest* request,
                           uint8_t* data, size_t len,
                           lightwaveos::actors::ActorSystem& actorSystem,
                           std::function<void()> broadcastStatus);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

