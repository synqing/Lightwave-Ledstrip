#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>
#include "../../../effects/zones/ZoneComposer.h"
#include "../../ApiResponse.h"

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

class ZoneHandlers {
public:
    static void handleList(AsyncWebServerRequest* request, lightwaveos::actors::ActorSystem& actors, lightwaveos::actors::RendererActor* renderer, lightwaveos::zones::ZoneComposer* composer);
    static void handleLayout(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleGet(AsyncWebServerRequest* request, lightwaveos::actors::ActorSystem& actors, lightwaveos::actors::RendererActor* renderer, lightwaveos::zones::ZoneComposer* composer);
    static void handleSetEffect(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::actors::ActorSystem& actors, lightwaveos::actors::RendererActor* renderer, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetBrightness(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetSpeed(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetPalette(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetBlend(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);
    static void handleSetEnabled(AsyncWebServerRequest* request, uint8_t* data, size_t len, lightwaveos::zones::ZoneComposer* composer, std::function<void()> broadcastZoneState);

private:
    static uint8_t extractZoneIdFromPath(AsyncWebServerRequest* request);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
