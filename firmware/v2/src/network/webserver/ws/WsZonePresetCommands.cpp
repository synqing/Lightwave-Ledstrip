/**
 * @file WsZonePresetCommands.cpp
 * @brief WebSocket zone preset command handlers implementation
 *
 * Commands:
 * - zonePresets.list    : List all zone presets
 * - zonePresets.get     : Get specific preset by id
 * - zonePresets.saveCurrent : Save current zone config to slot
 * - zonePresets.load    : Load and apply preset
 * - zonePresets.delete  : Delete preset from slot
 */

#include "WsZonePresetCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/persistence/ZonePresetManager.h"
#include "../../../effects/zones/ZoneComposer.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::persistence;

// ==================== Handler: zonePresets.list ====================

/**
 * @brief List all zone presets
 *
 * Request: {"type": "zonePresets.list"}
 * Response: {"type": "zonePresets.list", "success": true, "data": {"presets": [...], "count": N}}
 */
static void handleZonePresetsList(AsyncWebSocketClient* client, JsonDocument& doc,
                                   const WebServerContext& ctx) {
    (void)ctx;
    const char* requestId = doc["requestId"] | "";

    // Ensure preset manager is initialised
    if (!ZONE_PRESET_MANAGER.isInitialized()) {
        if (!ZONE_PRESET_MANAGER.init()) {
            client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                       "Zone preset system not initialised", requestId));
            return;
        }
    }

    // List all presets
    ZonePresetMetadata metadata[ZONE_PRESET_MAX_SLOTS];
    uint8_t count = ZONE_PRESET_MAX_SLOTS;
    NVSResult result = ZONE_PRESET_MANAGER.list(metadata, count);

    if (result != NVSResult::OK) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                   "Failed to list zone presets", requestId));
        return;
    }

    String response = buildWsResponse("zonePresets.list", requestId,
        [&metadata, count](JsonObject& data) {
            JsonArray presets = data["presets"].to<JsonArray>();
            for (uint8_t i = 0; i < count; i++) {
                const ZonePresetMetadata& meta = metadata[i];
                JsonObject preset = presets.add<JsonObject>();
                preset["id"] = meta.slot;
                preset["name"] = meta.name;
                preset["zoneCount"] = meta.zoneCount;
                preset["timestamp"] = meta.timestamp;
            }
            data["count"] = count;
            data["maxSlots"] = ZONE_PRESET_MAX_SLOTS;
        });
    client->text(response);
}

// ==================== Handler: zonePresets.get ====================

/**
 * @brief Get a specific zone preset
 *
 * Request: {"type": "zonePresets.get", "id": 0}
 * Response: {"type": "zonePresets.get", "success": true, "data": {"preset": {...}}}
 */
static void handleZonePresetsGet(AsyncWebSocketClient* client, JsonDocument& doc,
                                  const WebServerContext& ctx) {
    (void)ctx;
    const char* requestId = doc["requestId"] | "";

    // Validate required field
    if (!doc.containsKey("id")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD,
                                   "Missing 'id' field", requestId));
        return;
    }

    uint8_t slotId = doc["id"].as<uint8_t>();

    // Validate slot range
    if (slotId >= ZONE_PRESET_MAX_SLOTS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE,
                                   "Invalid preset id (valid: 0-15)", requestId));
        return;
    }

    // Ensure preset manager is initialised
    if (!ZONE_PRESET_MANAGER.isInitialized()) {
        if (!ZONE_PRESET_MANAGER.init()) {
            client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                       "Zone preset system not initialised", requestId));
            return;
        }
    }

    // Load preset
    ZonePreset preset;
    NVSResult result = ZONE_PRESET_MANAGER.load(slotId, preset);

    if (result == NVSResult::NOT_FOUND) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND,
                                   "Preset not found", requestId));
        return;
    }

    if (result != NVSResult::OK) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                   "Failed to load preset", requestId));
        return;
    }

    String response = buildWsResponse("zonePresets.get", requestId,
        [&preset, slotId](JsonObject& data) {
            JsonObject presetObj = data["preset"].to<JsonObject>();
            presetObj["id"] = slotId;
            presetObj["name"] = preset.name;
            presetObj["zoneCount"] = preset.zoneCount;
            presetObj["timestamp"] = preset.timestamp;

            JsonArray zones = presetObj["zones"].to<JsonArray>();
            for (uint8_t i = 0; i < preset.zoneCount; i++) {
                const ZonePresetEntry& entry = preset.zones[i];
                JsonObject zone = zones.add<JsonObject>();
                zone["zoneId"] = i;
                zone["effectId"] = entry.effectId;
                zone["paletteId"] = entry.paletteId;
                zone["brightness"] = entry.brightness;
                zone["speed"] = entry.speed;
                zone["blendMode"] = entry.blendMode;

                JsonObject segment = zone["segment"].to<JsonObject>();
                segment["s1LeftStart"] = entry.s1LeftStart;
                segment["s1LeftEnd"] = entry.s1LeftEnd;
                segment["s1RightStart"] = entry.s1RightStart;
                segment["s1RightEnd"] = entry.s1RightEnd;
            }
        });
    client->text(response);
}

// ==================== Handler: zonePresets.saveCurrent ====================

/**
 * @brief Save current zone configuration as a preset
 *
 * Request: {"type": "zonePresets.saveCurrent", "slot": 0, "name": "My Zones"}
 * Response: {"type": "zonePresets.saved", "success": true, "data": {"slot": 0, "preset": {...}}}
 * Broadcast: {"type": "zonePresets.saved", ...} to all clients
 */
static void handleZonePresetsSaveCurrent(AsyncWebSocketClient* client, JsonDocument& doc,
                                          const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    // Validate required fields
    if (!doc.containsKey("slot")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD,
                                   "Missing 'slot' field", requestId));
        return;
    }

    uint8_t slotId = doc["slot"].as<uint8_t>();
    const char* name = doc["name"] | "Untitled";

    // Validate slot range
    if (slotId >= ZONE_PRESET_MAX_SLOTS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE,
                                   "Invalid slot (valid: 0-15)", requestId));
        return;
    }

    // Check zone composer availability
    if (!ctx.zoneComposer) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED,
                                   "Zone system not available", requestId));
        return;
    }

    // Ensure preset manager is initialised
    if (!ZONE_PRESET_MANAGER.isInitialized()) {
        if (!ZONE_PRESET_MANAGER.init()) {
            client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                       "Zone preset system not initialised", requestId));
            return;
        }
    }

    // Save current zone configuration
    NVSResult result = ZONE_PRESET_MANAGER.saveCurrentZones(slotId, name, ctx.zoneComposer);

    if (result != NVSResult::OK) {
        const char* errMsg = (result == NVSResult::WRITE_ERROR)
                             ? "Storage write failed"
                             : "Failed to save preset";
        client->text(buildWsError(ErrorCodes::OPERATION_FAILED, errMsg, requestId));
        return;
    }

    // Get zone count for response
    uint8_t zoneCount = ctx.zoneComposer->getZoneCount();

    // Build success response and broadcast to all clients
    String response = buildWsResponse("zonePresets.saved", requestId,
        [slotId, name, zoneCount](JsonObject& data) {
            data["slot"] = slotId;
            JsonObject preset = data["preset"].to<JsonObject>();
            preset["id"] = slotId;
            preset["name"] = name;
            preset["zoneCount"] = zoneCount;
        });

    // Broadcast to all connected clients (including requester)
    if (ctx.ws) {
        ctx.ws->textAll(response);
    } else {
        client->text(response);
    }
}

// ==================== Handler: zonePresets.load ====================

/**
 * @brief Load and apply a zone preset
 *
 * Request: {"type": "zonePresets.load", "id": 0}
 * Response: {"type": "zonePresets.loaded", "success": true, "data": {"id": 0, "name": "..."}}
 * Broadcast: Zone state change broadcast
 */
static void handleZonePresetsLoad(AsyncWebSocketClient* client, JsonDocument& doc,
                                   const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    // Validate required field
    if (!doc.containsKey("id")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD,
                                   "Missing 'id' field", requestId));
        return;
    }

    uint8_t slotId = doc["id"].as<uint8_t>();

    // Validate slot range
    if (slotId >= ZONE_PRESET_MAX_SLOTS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE,
                                   "Invalid preset id (valid: 0-15)", requestId));
        return;
    }

    // Check zone composer availability
    if (!ctx.zoneComposer) {
        client->text(buildWsError(ErrorCodes::FEATURE_DISABLED,
                                   "Zone system not available", requestId));
        return;
    }

    // Ensure preset manager is initialised
    if (!ZONE_PRESET_MANAGER.isInitialized()) {
        if (!ZONE_PRESET_MANAGER.init()) {
            client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                       "Zone preset system not initialised", requestId));
            return;
        }
    }

    // Load preset to get name before applying
    ZonePreset preset;
    NVSResult result = ZONE_PRESET_MANAGER.load(slotId, preset);

    if (result == NVSResult::NOT_FOUND) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND,
                                   "Preset not found", requestId));
        return;
    }

    if (result != NVSResult::OK) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                   "Failed to load preset", requestId));
        return;
    }

    // Apply preset to zone composer
    result = ZONE_PRESET_MANAGER.applyToZones(slotId, ctx.zoneComposer);

    if (result != NVSResult::OK) {
        client->text(buildWsError(ErrorCodes::OPERATION_FAILED,
                                   "Failed to apply preset", requestId));
        return;
    }

    // Build success response
    String response = buildWsResponse("zonePresets.loaded", requestId,
        [slotId, &preset](JsonObject& data) {
            data["id"] = slotId;
            data["name"] = preset.name;
            data["zoneCount"] = preset.zoneCount;
        });
    client->text(response);

    // Broadcast zone state change to all clients
    if (ctx.broadcastZoneState) {
        ctx.broadcastZoneState();
    }
}

// ==================== Handler: zonePresets.delete ====================

/**
 * @brief Delete a zone preset
 *
 * Request: {"type": "zonePresets.delete", "id": 0}
 * Response: {"type": "zonePresets.deleted", "success": true, "data": {"id": 0}}
 * Broadcast: {"type": "zonePresets.deleted", ...} to all clients
 */
static void handleZonePresetsDelete(AsyncWebSocketClient* client, JsonDocument& doc,
                                     const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    // Validate required field
    if (!doc.containsKey("id")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD,
                                   "Missing 'id' field", requestId));
        return;
    }

    uint8_t slotId = doc["id"].as<uint8_t>();

    // Validate slot range
    if (slotId >= ZONE_PRESET_MAX_SLOTS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE,
                                   "Invalid preset id (valid: 0-15)", requestId));
        return;
    }

    // Ensure preset manager is initialised
    if (!ZONE_PRESET_MANAGER.isInitialized()) {
        if (!ZONE_PRESET_MANAGER.init()) {
            client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                       "Zone preset system not initialised", requestId));
            return;
        }
    }

    // Check if slot is occupied
    if (!ZONE_PRESET_MANAGER.isSlotOccupied(slotId)) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND,
                                   "Preset not found", requestId));
        return;
    }

    // Remove preset
    NVSResult result = ZONE_PRESET_MANAGER.remove(slotId);

    if (result != NVSResult::OK) {
        client->text(buildWsError(ErrorCodes::OPERATION_FAILED,
                                   "Failed to delete preset", requestId));
        return;
    }

    // Build success response and broadcast to all clients
    String response = buildWsResponse("zonePresets.deleted", requestId,
        [slotId](JsonObject& data) {
            data["id"] = slotId;
        });

    // Broadcast to all connected clients
    if (ctx.ws) {
        ctx.ws->textAll(response);
    } else {
        client->text(response);
    }
}

// ==================== Command Registration ====================

void registerWsZonePresetCommands(const WebServerContext& ctx) {
    (void)ctx;

    WsCommandRouter::registerCommand("zonePresets.list", handleZonePresetsList);
    WsCommandRouter::registerCommand("zonePresets.get", handleZonePresetsGet);
    WsCommandRouter::registerCommand("zonePresets.saveCurrent", handleZonePresetsSaveCurrent);
    WsCommandRouter::registerCommand("zonePresets.load", handleZonePresetsLoad);
    WsCommandRouter::registerCommand("zonePresets.delete", handleZonePresetsDelete);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
