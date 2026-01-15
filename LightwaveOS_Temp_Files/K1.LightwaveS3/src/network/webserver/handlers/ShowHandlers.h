#pragma once

#include "../HttpRouteRegistry.h"
#include "../../ApiResponse.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace nodes {
class NodeOrchestrator;
class RendererNode;
}
}

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class ShowHandlers {
public:
    /**
     * @brief List all shows (builtin + custom)
     * GET /api/v1/shows
     */
    static void handleList(AsyncWebServerRequest* request,
                          lightwaveos::nodes::NodeOrchestrator& orchestrator);

    /**
     * @brief Get show details
     * GET /api/v1/shows/{id}?format=scenes|cues
     */
    static void handleGet(AsyncWebServerRequest* request,
                         const String& showId,
                         const String& format,
                         lightwaveos::nodes::NodeOrchestrator& orchestrator);

    /**
     * @brief Create new custom show
     * POST /api/v1/shows
     */
    static void handleCreate(AsyncWebServerRequest* request,
                             uint8_t* data, size_t len,
                             lightwaveos::nodes::NodeOrchestrator& orchestrator);

    /**
     * @brief Update existing custom show
     * PUT /api/v1/shows/{id}
     */
    static void handleUpdate(AsyncWebServerRequest* request,
                            const String& showId,
                            uint8_t* data, size_t len,
                            lightwaveos::nodes::NodeOrchestrator& orchestrator);

    /**
     * @brief Delete custom show
     * DELETE /api/v1/shows/{id}
     */
    static void handleDelete(AsyncWebServerRequest* request,
                            const String& showId,
                            lightwaveos::nodes::NodeOrchestrator& orchestrator);

    /**
     * @brief Get current show playback state
     * GET /api/v1/shows/current
     */
    static void handleCurrent(AsyncWebServerRequest* request,
                              lightwaveos::nodes::NodeOrchestrator& orchestrator);

    /**
     * @brief Control show playback
     * POST /api/v1/shows/control
     */
    static void handleControl(AsyncWebServerRequest* request,
                             uint8_t* data, size_t len,
                             lightwaveos::nodes::NodeOrchestrator& orchestrator);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

