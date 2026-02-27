/**
 * @file ModifierHandlers.h
 * @brief Effect modifier REST API handlers (stub)
 */

#pragma once

#include <ESPAsyncWebServer.h>

namespace lightwaveos {
namespace actors { class RendererActor; }
namespace network {
namespace webserver {
namespace handlers {

class ModifierHandlers {
public:
    static void handleListModifiers(AsyncWebServerRequest* request, actors::RendererActor* renderer);
    static void handleAddModifier(AsyncWebServerRequest* request, uint8_t* data, size_t len, actors::RendererActor* renderer);
    static void handleRemoveModifier(AsyncWebServerRequest* request, uint8_t* data, size_t len, actors::RendererActor* renderer);
    static void handleClearModifiers(AsyncWebServerRequest* request, actors::RendererActor* renderer);
    static void handleUpdateModifier(AsyncWebServerRequest* request, uint8_t* data, size_t len, actors::RendererActor* renderer);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
