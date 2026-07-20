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

    /// POST /api/v1/transitions/leadtime — Phase 2.4
    /// Body: { type: uint8 (0..11, required), duration: uint16 (optional, 0 = tier default) }
    /// Returns: { leadTime, duration, safetyMargin: 50, tier }
    static void handleLeadtime(AsyncWebServerRequest* request,
                                uint8_t* data, size_t len,
                                lightwaveos::actors::ActorSystem& actors);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
