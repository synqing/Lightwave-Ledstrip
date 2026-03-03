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

class StimulusHandlers {
public:
    static void handleMode(AsyncWebServerRequest* request,
                           uint8_t* data, size_t len,
                           lightwaveos::actors::ActorSystem& actorSystem,
                           lightwaveos::actors::RendererActor* renderer);

    static void handlePatch(AsyncWebServerRequest* request,
                            uint8_t* data, size_t len,
                            lightwaveos::actors::ActorSystem& actorSystem,
                            lightwaveos::actors::RendererActor* renderer);

    static void handleClear(AsyncWebServerRequest* request,
                            lightwaveos::actors::ActorSystem& actorSystem,
                            lightwaveos::actors::RendererActor* renderer);

    static void handleStatus(AsyncWebServerRequest* request,
                             lightwaveos::actors::ActorSystem& actorSystem,
                             lightwaveos::actors::RendererActor* renderer);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

