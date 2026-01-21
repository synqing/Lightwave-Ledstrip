/**
 * @file PaletteHandlers.h
 * @brief Palette-related HTTP handlers
 */

#pragma once

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

class PaletteHandlers {
public:
    static void handleList(AsyncWebServerRequest* request,
                           lightwaveos::actors::RendererActor* renderer);
    
    static void handleCurrent(AsyncWebServerRequest* request,
                               lightwaveos::actors::RendererActor* renderer);
    
    static void handleSet(AsyncWebServerRequest* request,
                          uint8_t* data, size_t len,
                          lightwaveos::actors::ActorSystem& actorSystem,
                          std::function<void()> broadcastStatus);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

