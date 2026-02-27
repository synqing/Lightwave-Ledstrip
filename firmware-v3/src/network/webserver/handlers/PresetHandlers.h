/**
 * @file PresetHandlers.h
 * @brief Preset management REST API handlers (stub)
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <functional>

namespace lightwaveos {
namespace persistence { class PresetManager; class ZoneConfigManager; }
namespace zones { class ZoneComposer; }
namespace network {
namespace webserver {
namespace handlers {

class PresetHandlers {
public:
    static void handleList(AsyncWebServerRequest* request, persistence::PresetManager* mgr);
    static void handleGet(AsyncWebServerRequest* request, persistence::PresetManager* mgr);
    static void handleSave(AsyncWebServerRequest* request, uint8_t* data, size_t len, persistence::PresetManager* mgr);
    static void handleUpdate(AsyncWebServerRequest* request, uint8_t* data, size_t len, persistence::PresetManager* mgr);
    static void handleDelete(AsyncWebServerRequest* request, persistence::PresetManager* mgr);
    static void handleRename(AsyncWebServerRequest* request, uint8_t* data, size_t len, persistence::PresetManager* mgr);
    static void handleLoad(AsyncWebServerRequest* request, zones::ZoneComposer* composer, persistence::ZoneConfigManager* zoneConfigMgr, persistence::PresetManager* presetMgr, std::function<void()> broadcastFn);
    static void handleSaveCurrent(AsyncWebServerRequest* request, uint8_t* data, size_t len, zones::ZoneComposer* composer, persistence::ZoneConfigManager* zoneConfigMgr, persistence::PresetManager* presetMgr);
};

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
