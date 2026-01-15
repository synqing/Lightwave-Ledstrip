/**
 * @file ZonePresetHandlers.h
 * @brief Zone preset HTTP handlers
 *
 * REST API handlers for zone preset CRUD operations.
 *
 * Endpoints:
 * - GET  /api/v1/zone-presets       - List all presets
 * - GET  /api/v1/zone-presets/get   - Get preset details by id
 * - POST /api/v1/zone-presets       - Save new preset
 * - POST /api/v1/zone-presets/apply - Apply preset by id
 * - DELETE /api/v1/zone-presets/delete - Delete preset by id
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Forward declarations
namespace lightwaveos {
namespace nodes {
class NodeOrchestrator;
class RendererNode;
}
namespace zones {
class ZoneComposer;
}
}

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

/**
 * @brief Zone preset HTTP handlers
 *
 * Provides REST API endpoints for managing zone configuration presets.
 * Presets store complete zone state including zone count, per-zone
 * effect settings, brightness, speed, palette, and blend modes.
 */
class ZonePresetHandlers {
public:
    /**
     * @brief List all saved zone presets
     * GET /api/v1/zone-presets
     */
    static void handleList(AsyncWebServerRequest* request);

    /**
     * @brief Get a specific preset by ID
     * GET /api/v1/zone-presets/get?id=N
     */
    static void handleGet(AsyncWebServerRequest* request, uint8_t presetId);

    /**
     * @brief Save current zone config as new preset
     * POST /api/v1/zone-presets
     * Body: {"name": "preset name"}
     */
    static void handleSave(AsyncWebServerRequest* request,
                           uint8_t* data, size_t len,
                           lightwaveos::zones::ZoneComposer* zoneComposer);

    /**
     * @brief Apply a preset by ID
     * POST /api/v1/zone-presets/apply?id=N
     */
    static void handleApply(AsyncWebServerRequest* request, uint8_t presetId,
                            lightwaveos::nodes::NodeOrchestrator& orchestrator,
                            lightwaveos::zones::ZoneComposer* zoneComposer,
                            std::function<void()> broadcastZoneState);

    /**
     * @brief Delete a preset by ID
     * DELETE /api/v1/zone-presets/delete?id=N
     */
    static void handleDelete(AsyncWebServerRequest* request, uint8_t presetId);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
