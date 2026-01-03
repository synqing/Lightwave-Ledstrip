/**
 * @file EffectPresetHandlers.h
 * @brief Effect preset HTTP handlers
 *
 * REST API handlers for effect preset CRUD operations.
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Forward declarations
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

/**
 * @brief Effect preset HTTP handlers
 *
 * Endpoints:
 * - GET  /api/v1/effect-presets       - List all presets
 * - GET  /api/v1/effect-presets/get   - Get preset details by id
 * - POST /api/v1/effect-presets       - Save new preset
 * - POST /api/v1/effect-presets/apply - Apply preset by id
 * - DELETE /api/v1/effect-presets/delete - Delete preset by id
 */
class EffectPresetHandlers {
public:
    /**
     * @brief List all saved effect presets
     * GET /api/v1/effect-presets
     */
    static void handleList(AsyncWebServerRequest* request);

    /**
     * @brief Get a specific preset by ID
     * GET /api/v1/effect-presets/get?id=N
     */
    static void handleGet(AsyncWebServerRequest* request, uint8_t presetId);

    /**
     * @brief Save current effect config as new preset
     * POST /api/v1/effect-presets
     * Body: {"name": "preset name"}
     */
    static void handleSave(AsyncWebServerRequest* request,
                           uint8_t* data, size_t len,
                           lightwaveos::actors::RendererActor* renderer);

    /**
     * @brief Apply a preset by ID
     * POST /api/v1/effect-presets/apply?id=N
     */
    static void handleApply(AsyncWebServerRequest* request, uint8_t presetId,
                            lightwaveos::actors::ActorSystem& actorSystem,
                            lightwaveos::actors::RendererActor* renderer);

    /**
     * @brief Delete a preset by ID
     * DELETE /api/v1/effect-presets/delete?id=N
     */
    static void handleDelete(AsyncWebServerRequest* request, uint8_t presetId);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
