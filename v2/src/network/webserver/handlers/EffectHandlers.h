#pragma once

#include "../HttpRouteRegistry.h"
#include "../../ApiResponse.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

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

class EffectHandlers {
public:
    static void registerRoutes(HttpRouteRegistry& registry);

    static void handleList(AsyncWebServerRequest* request, lightwaveos::actors::RendererActor* renderer);
    static void handleCurrent(AsyncWebServerRequest* request, lightwaveos::actors::RendererActor* renderer);
    static void handleSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::actors::ActorSystem& actors, lightwaveos::actors::RendererActor* renderer, std::function<void()> broadcastStatus);
    static void handleMetadata(AsyncWebServerRequest* request, lightwaveos::actors::RendererActor* renderer);
    static void handleFamilies(AsyncWebServerRequest* request);
    static void handleParametersGet(AsyncWebServerRequest* request, lightwaveos::actors::RendererActor* renderer);
    static void handleParametersSet(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::actors::RendererActor* renderer);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
