// SPDX-License-Identifier: Apache-2.0
// Copyright 2025-2026 SpectraSynq
/**
 * @file TransitionHandlers.h
 * @brief Transition-related HTTP handlers
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

class TransitionHandlers {
public:
    static void handleTypes(AsyncWebServerRequest* request);
    
    static void handleTrigger(AsyncWebServerRequest* request,
                               uint8_t* data, size_t len,
                               lightwaveos::actors::ActorSystem& actors,
                               const lightwaveos::network::WebServer::CachedRendererState& cachedState,
                               std::function<void()> broadcastStatus);
    
    static void handleConfigGet(AsyncWebServerRequest* request,
                                 lightwaveos::actors::RendererActor* renderer);
    
    static void handleConfigSet(AsyncWebServerRequest* request,
                                 uint8_t* data, size_t len);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
