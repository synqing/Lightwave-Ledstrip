#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>
#include "../../../effects/zones/ZoneComposer.h"
#include "../../../core/persistence/ZoneConfigManager.h"
#include "../../../core/persistence/PresetManager.h"
#include "../../ApiResponse.h"
#include "../../WebServer.h"  // For CachedRendererState

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

class PresetHandlers {
public:
    /**
     * @brief GET /api/v1/presets - List all saved presets
     */
    static void handleList(AsyncWebServerRequest* request, lightwaveos::persistence::PresetManager* presetManager);

    /**
     * @brief GET /api/v1/presets/{name} - Download preset as JSON file
     */
    static void handleGet(AsyncWebServerRequest* request, lightwaveos::persistence::PresetManager* presetManager);

    /**
     * @brief POST /api/v1/presets - Upload/save new preset (JSON body)
     */
    static void handleSave(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                           lightwaveos::persistence::PresetManager* presetManager);

    /**
     * @brief PUT /api/v1/presets/{name} - Update existing preset
     */
    static void handleUpdate(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                             lightwaveos::persistence::PresetManager* presetManager);

    /**
     * @brief DELETE /api/v1/presets/{name} - Delete a preset
     */
    static void handleDelete(AsyncWebServerRequest* request, lightwaveos::persistence::PresetManager* presetManager);

    /**
     * @brief POST /api/v1/presets/{name}/rename - Rename a preset
     */
    static void handleRename(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                             lightwaveos::persistence::PresetManager* presetManager);

    /**
     * @brief POST /api/v1/presets/{name}/load - Load preset into active zone config
     */
    static void handleLoad(AsyncWebServerRequest* request,
                           lightwaveos::zones::ZoneComposer* composer,
                           lightwaveos::persistence::ZoneConfigManager* configManager,
                           lightwaveos::persistence::PresetManager* presetManager,
                           std::function<void()> broadcastZoneState);

    /**
     * @brief POST /api/v1/presets/save-current - Save current config as new preset
     */
    static void handleSaveCurrent(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                                  lightwaveos::zones::ZoneComposer* composer,
                                  lightwaveos::persistence::ZoneConfigManager* configManager,
                                  lightwaveos::persistence::PresetManager* presetManager);

private:
    /**
     * @brief Extract preset name from URL path
     * @param request HTTP request
     * @return Preset name or empty string if not found
     */
    static String extractPresetNameFromPath(AsyncWebServerRequest* request);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos

