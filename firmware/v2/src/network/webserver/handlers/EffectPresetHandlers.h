/**
 * @file EffectPresetHandlers.h
 * @brief Effect preset REST API handlers (stub)
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include "../../../core/actors/RendererActor.h"

namespace lightwaveos {
namespace actors { class ActorSystem; }
namespace network {
namespace webserver {
namespace handlers {

class EffectPresetHandlers {
public:
    static void handleList(AsyncWebServerRequest* request);
    static void handleSave(AsyncWebServerRequest* request, uint8_t* data, size_t len, actors::RendererActor* renderer);
    static void handleGet(AsyncWebServerRequest* request, uint8_t id);
    static void handleApply(AsyncWebServerRequest* request, uint8_t id, actors::ActorSystem& orchestrator, actors::RendererActor* renderer);
    static void handleDelete(AsyncWebServerRequest* request, uint8_t id);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
