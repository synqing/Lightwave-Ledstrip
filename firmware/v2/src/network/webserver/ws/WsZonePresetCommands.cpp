/**
 * @file WsZonePresetCommands.cpp
 * @brief WebSocket zone preset command handlers implementation
 *
 * LightwaveOS v2 - WebSocket API
 *
 * Implements zone preset CRUD operations over WebSocket.
 */

#include "WsZonePresetCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../core/persistence/ZonePreset.h"
#include "../../../effects/zones/ZoneComposer.h"
#include "../../../effects/zones/BlendMode.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::persistence;
using namespace lightwaveos::zones;

/**
 * @brief Handle zonePresets.list command
 *
 * Lists all saved zone presets with their names and IDs.
 *
 * Request: {"type": "zonePresets.list", "requestId": "..."}
 * Response: {"type": "zonePresets.list", "success": true, "data": {
 *   "count": 2, "maxPresets": 5, "presets": [{"id": 0, "name": "..."}, ...]
 * }}
 */
static void handleZonePresetsList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    auto& mgr = ZonePresetManager::instance();

    char names[ZonePresetManager::MAX_PRESETS][ZonePreset::NAME_MAX_LEN];
    uint8_t ids[ZonePresetManager::MAX_PRESETS];
    uint8_t count = mgr.listPresets(names, ids);

    String response = buildWsResponse("zonePresets.list", requestId, [count, &names, &ids](JsonObject& data) {
        data["count"] = count;
        data["maxPresets"] = ZonePresetManager::MAX_PRESETS;
        JsonArray presets = data["presets"].to<JsonArray>();
        for (uint8_t i = 0; i < count; i++) {
            JsonObject p = presets.add<JsonObject>();
            p["id"] = ids[i];
            p["name"] = names[i];
        }
    });
    client->text(response);
}

/**
 * @brief Handle zonePresets.get command
 *
 * Gets details of a specific zone preset by ID.
 *
 * Request: {"type": "zonePresets.get", "id": 0, "requestId": "..."}
 * Response: {"type": "zonePresets.get", "success": true, "data": {
 *   "id": 0, "name": "...", "zoneCount": 3, "zonesEnabled": true, "zones": [...], "segments": [...]
 * }}
 */
static void handleZonePresetsGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint8_t presetId = doc["id"] | 255;

    if (presetId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "id required", requestId));
        return;
    }

    if (presetId >= ZonePresetManager::MAX_PRESETS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-4", requestId));
        return;
    }

    auto& mgr = ZonePresetManager::instance();
    ZonePreset preset;
    if (!mgr.getPreset(presetId, preset)) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND, "Preset not found", requestId));
        return;
    }

    String response = buildWsResponse("zonePresets.get", requestId, [presetId, &preset](JsonObject& data) {
        data["id"] = presetId;
        data["name"] = preset.name;
        data["zonesEnabled"] = preset.zonesEnabled;
        data["zoneCount"] = preset.zoneCount;

        // Per-zone configuration
        JsonArray zonesArr = data["zones"].to<JsonArray>();
        for (uint8_t i = 0; i < preset.zoneCount && i < ZonePreset::MAX_ZONES; i++) {
            JsonObject z = zonesArr.add<JsonObject>();
            z["enabled"] = preset.zones[i].enabled;
            z["effectId"] = preset.zones[i].effectId;
            z["brightness"] = preset.zones[i].brightness;
            z["speed"] = preset.zones[i].speed;
            z["paletteId"] = preset.zones[i].paletteId;
            z["blendMode"] = preset.zones[i].blendMode;
            z["blendModeName"] = getBlendModeName(static_cast<BlendMode>(preset.zones[i].blendMode));
        }

        // Segment layout
        JsonArray segmentsArr = data["segments"].to<JsonArray>();
        for (uint8_t i = 0; i < preset.zoneCount && i < ZonePreset::MAX_ZONES; i++) {
            JsonObject s = segmentsArr.add<JsonObject>();
            s["leftStart"] = preset.segments[i].s1LeftStart;
            s["leftEnd"] = preset.segments[i].s1LeftEnd;
            s["rightStart"] = preset.segments[i].s1RightStart;
            s["rightEnd"] = preset.segments[i].s1RightEnd;
        }
    });
    client->text(response);
}

/**
 * @brief Handle zonePresets.save command
 *
 * Saves current zone configuration as a new preset.
 *
 * Request: {"type": "zonePresets.save", "name": "My Zones", "requestId": "..."}
 *   Optional: "id" to overwrite specific slot
 * Response: {"type": "zonePresets.save", "success": true, "data": {
 *   "id": 0, "name": "My Zones", "zoneCount": 3, "message": "Zone preset saved"
 * }}
 */
static void handleZonePresetsSave(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    if (!ctx.zoneComposer) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }

    // Get preset name (required)
    const char* name = doc["name"] | "Unnamed";

    auto& mgr = ZonePresetManager::instance();
    int8_t slotId = -1;

    // Check if saving to specific slot
    if (doc.containsKey("id")) {
        uint8_t requestedId = doc["id"].as<uint8_t>();
        if (requestedId < ZonePresetManager::MAX_PRESETS) {
            if (mgr.savePresetAt(requestedId, name, ctx.zoneComposer)) {
                slotId = requestedId;
            }
        } else {
            client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-4", requestId));
            return;
        }
    } else {
        slotId = mgr.savePreset(name, ctx.zoneComposer);
    }

    if (slotId < 0) {
        client->text(buildWsError(ErrorCodes::STORAGE_FULL, "No free preset slots", requestId));
        return;
    }

    // Get saved preset for response
    ZonePreset savedPreset;
    mgr.getPreset(slotId, savedPreset);

    String response = buildWsResponse("zonePresets.save", requestId,
        [slotId, &savedPreset](JsonObject& data) {
            data["id"] = slotId;
            data["name"] = savedPreset.name;
            data["zoneCount"] = savedPreset.zoneCount;
            data["zonesEnabled"] = savedPreset.zonesEnabled;
            data["message"] = "Zone preset saved";
        });
    client->text(response);
}

/**
 * @brief Handle zonePresets.apply command
 *
 * Applies a saved zone preset, restoring complete zone configuration.
 *
 * Request: {"type": "zonePresets.apply", "id": 0, "requestId": "..."}
 * Response: {"type": "zonePresets.apply", "success": true, "data": {
 *   "id": 0, "name": "...", "zoneCount": 3, "message": "Zone preset applied"
 * }}
 */
static void handleZonePresetsApply(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint8_t presetId = doc["id"] | 255;

    if (presetId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "id required", requestId));
        return;
    }

    if (!ctx.zoneComposer) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED, "Zone system not available", requestId));
        return;
    }

    if (presetId >= ZonePresetManager::MAX_PRESETS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-4", requestId));
        return;
    }

    auto& mgr = ZonePresetManager::instance();
    ZonePreset preset;
    if (!mgr.getPreset(presetId, preset)) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND, "Preset not found", requestId));
        return;
    }

    // Apply preset
    if (!mgr.applyPreset(presetId, ctx.zoneComposer, ctx.actorSystem)) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "Failed to apply preset", requestId));
        return;
    }

    // Broadcast zone state update
    if (ctx.broadcastZoneState) ctx.broadcastZoneState();

    String response = buildWsResponse("zonePresets.apply", requestId, [presetId, &preset](JsonObject& data) {
        data["id"] = presetId;
        data["name"] = preset.name;
        data["zoneCount"] = preset.zoneCount;
        data["zonesEnabled"] = preset.zonesEnabled;
        data["message"] = "Zone preset applied";
    });
    client->text(response);
}

/**
 * @brief Handle zonePresets.delete command
 *
 * Deletes a saved zone preset by ID.
 *
 * Request: {"type": "zonePresets.delete", "id": 0, "requestId": "..."}
 * Response: {"type": "zonePresets.delete", "success": true, "data": {"id": 0, "message": "Zone preset deleted"}}
 */
static void handleZonePresetsDelete(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint8_t presetId = doc["id"] | 255;

    if (presetId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "id required", requestId));
        return;
    }

    if (presetId >= ZonePresetManager::MAX_PRESETS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-4", requestId));
        return;
    }

    auto& mgr = ZonePresetManager::instance();
    if (!mgr.hasPreset(presetId)) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND, "Preset not found", requestId));
        return;
    }

    if (!mgr.deletePreset(presetId)) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "Failed to delete preset", requestId));
        return;
    }

    String response = buildWsResponse("zonePresets.delete", requestId, [presetId](JsonObject& data) {
        data["id"] = presetId;
        data["message"] = "Zone preset deleted";
    });
    client->text(response);
}

void registerWsZonePresetCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("zonePresets.list", handleZonePresetsList);
    WsCommandRouter::registerCommand("zonePresets.get", handleZonePresetsGet);
    WsCommandRouter::registerCommand("zonePresets.save", handleZonePresetsSave);
    WsCommandRouter::registerCommand("zonePresets.apply", handleZonePresetsApply);
    WsCommandRouter::registerCommand("zonePresets.delete", handleZonePresetsDelete);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
