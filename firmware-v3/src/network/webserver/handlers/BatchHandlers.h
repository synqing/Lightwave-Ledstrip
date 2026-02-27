/**
 * @file BatchHandlers.h
 * @brief Batch operation HTTP handlers
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>

namespace lightwaveos {
namespace actors {
class ActorSystem;
}
}

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class BatchHandlers {
public:
    static void handleExecute(AsyncWebServerRequest* request,
                                uint8_t* data, size_t len,
                                lightwaveos::actors::ActorSystem& actorSystem,
                                std::function<bool(const String&, JsonVariant)> executeBatchAction,
                                std::function<void()> broadcastStatus);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

