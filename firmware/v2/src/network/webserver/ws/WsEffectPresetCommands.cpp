/**
 * @file WsEffectPresetCommands.cpp
 * @brief WebSocket effect preset command handlers implementation
 *
 * LightwaveOS v2 - WebSocket API
 *
 * Implements CRUD operations for effect presets via WebSocket:
 * - effectPresets.list   : Returns all preset slots with metadata
 * - effectPresets.get    : Returns full preset data for a slot
 * - effectPresets.saveCurrent : Captures current effect state to NVS
 * - effectPresets.load   : Loads preset and applies to RendererActor
 * - effectPresets.delete : Removes preset from NVS
 *
 * Thread-safe: All persistence operations use NVSManager internally.
 * Broadcasts: Changes are broadcast to all connected WebSocket clients.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#include "WsEffectPresetCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/persistence/EffectPresetManager.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

// ============================================================================
// Helper: Build preset JSON object
// ============================================================================

/**
 * @brief Serialise an EffectPreset to JSON
 * @param preset The preset data
 * @param slot Slot index
 * @param obj JSON object to populate
 */
static void buildPresetJson(const persistence::EffectPreset& preset,
                            uint8_t slot,
                            JsonObject& obj) {
    obj["id"] = slot;
    obj["name"] = preset.name;
    obj["effectId"] = preset.effectId;
    obj["paletteId"] = preset.paletteId;
    obj["brightness"] = preset.brightness;
    obj["speed"] = preset.speed;
    obj["hue"] = preset.hue;
    obj["saturation"] = preset.saturation;
    obj["intensity"] = preset.intensity;
    obj["complexity"] = preset.complexity;
    obj["variation"] = preset.variation;
    obj["mood"] = preset.mood;
    obj["trails"] = preset.trails;
    obj["timestamp"] = preset.timestamp;
}

/**
 * @brief Serialise metadata to JSON (lightweight list entry)
 * @param meta The metadata
 * @param obj JSON object to populate
 */
static void buildMetadataJson(const persistence::EffectPresetMetadata& meta,
                              JsonObject& obj) {
    obj["id"] = meta.slot;
    obj["name"] = meta.name;
    obj["effectId"] = meta.effectId;
    obj["paletteId"] = meta.paletteId;
    obj["timestamp"] = meta.timestamp;
    obj["occupied"] = meta.occupied;
}

// ============================================================================
// Command: effectPresets.list
// ============================================================================

/**
 * @brief List all effect presets
 *
 * Request:  {"type": "effectPresets.list", "requestId": "..."}
 * Response: {"type": "effectPresets.list", "data": {"presets": [...], "count": N, "maxSlots": 16}}
 */
static void handleEffectPresetsList(AsyncWebSocketClient* client,
                                    JsonDocument& doc,
                                    const WebServerContext& ctx) {
    (void)ctx;  // Not needed for list operation

    const char* requestId = doc["requestId"] | "";

    auto& mgr = persistence::EffectPresetManager::instance();
    if (!mgr.isInitialised()) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                  "EffectPresetManager not initialised", requestId));
        return;
    }

    // Fetch all preset metadata
    persistence::EffectPresetMetadata metadata[persistence::EffectPresetManager::MAX_PRESETS];
    uint8_t count = 0;
    persistence::NVSResult result = mgr.list(metadata, count);

    if (result != persistence::NVSResult::OK) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                  "Failed to list presets", requestId));
        return;
    }

    // Build response
    String response = buildWsResponse("effectPresets.list", requestId,
        [&metadata, count](JsonObject& data) {
            JsonArray presets = data["presets"].to<JsonArray>();
            for (uint8_t i = 0; i < persistence::EffectPresetManager::MAX_PRESETS; i++) {
                if (metadata[i].occupied) {
                    JsonObject preset = presets.add<JsonObject>();
                    buildMetadataJson(metadata[i], preset);
                }
            }
            data["count"] = count;
            data["maxSlots"] = persistence::EffectPresetManager::MAX_PRESETS;
        });

    client->text(response);
}

// ============================================================================
// Command: effectPresets.get
// ============================================================================

/**
 * @brief Get full preset data for a specific slot
 *
 * Request:  {"type": "effectPresets.get", "id": 0, "requestId": "..."}
 * Response: {"type": "effectPresets.get", "data": {"preset": {...}}}
 */
static void handleEffectPresetsGet(AsyncWebSocketClient* client,
                                   JsonDocument& doc,
                                   const WebServerContext& ctx) {
    (void)ctx;

    const char* requestId = doc["requestId"] | "";

    // Validate required parameter
    if (!doc.containsKey("id")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD,
                                  "Missing required field: id", requestId));
        return;
    }

    uint8_t slot = doc["id"] | 255;
    if (slot >= persistence::EffectPresetManager::MAX_PRESETS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE,
                                  "Slot id out of range (0-15)", requestId));
        return;
    }

    auto& mgr = persistence::EffectPresetManager::instance();
    if (!mgr.isInitialised()) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                  "EffectPresetManager not initialised", requestId));
        return;
    }

    // Load the preset
    persistence::EffectPreset preset;
    persistence::NVSResult result = mgr.load(slot, preset);

    if (result == persistence::NVSResult::NOT_FOUND) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND,
                                  "Preset slot is empty", requestId));
        return;
    }

    if (result != persistence::NVSResult::OK) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                  "Failed to load preset", requestId));
        return;
    }

    // Build response
    String response = buildWsResponse("effectPresets.get", requestId,
        [&preset, slot](JsonObject& data) {
            JsonObject presetObj = data["preset"].to<JsonObject>();
            buildPresetJson(preset, slot, presetObj);
        });

    client->text(response);
}

// ============================================================================
// Command: effectPresets.saveCurrent
// ============================================================================

/**
 * @brief Save current effect state to a preset slot
 *
 * Request:  {"type": "effectPresets.saveCurrent", "slot": 0, "name": "My Effect", "requestId": "..."}
 * Response: {"type": "effectPresets.saved", "data": {"slot": 0, "preset": {...}}}
 * Broadcast: {"type": "effectPresets.saved", "slot": 0, "preset": {...}}
 */
static void handleEffectPresetsSaveCurrent(AsyncWebSocketClient* client,
                                           JsonDocument& doc,
                                           const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    // Validate required parameters
    if (!doc.containsKey("slot")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD,
                                  "Missing required field: slot", requestId));
        return;
    }

    if (!doc.containsKey("name")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD,
                                  "Missing required field: name", requestId));
        return;
    }

    uint8_t slot = doc["slot"] | 255;
    if (slot >= persistence::EffectPresetManager::MAX_PRESETS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE,
                                  "Slot out of range (0-15)", requestId));
        return;
    }

    const char* name = doc["name"] | "";
    if (strlen(name) == 0) {
        client->text(buildWsError(ErrorCodes::INVALID_VALUE,
                                  "Preset name cannot be empty", requestId));
        return;
    }

    // Check renderer availability
    if (!ctx.renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY,
                                  "Renderer not available", requestId));
        return;
    }

    auto& mgr = persistence::EffectPresetManager::instance();
    if (!mgr.isInitialised()) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                  "EffectPresetManager not initialised", requestId));
        return;
    }

    // Save current effect state
    persistence::NVSResult result = mgr.saveCurrentEffect(slot, name, ctx.renderer);

    if (result == persistence::NVSResult::WRITE_ERROR) {
        // WRITE_ERROR can indicate NVS storage full or flash write failure
        client->text(buildWsError(ErrorCodes::STORAGE_FULL,
                                  "NVS storage full or write failed", requestId));
        return;
    }

    if (result != persistence::NVSResult::OK) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                  "Failed to save preset", requestId));
        return;
    }

    // Load back the saved preset for response
    persistence::EffectPreset savedPreset;
    mgr.load(slot, savedPreset);

    // Send response to requesting client
    String response = buildWsResponse("effectPresets.saved", requestId,
        [&savedPreset, slot](JsonObject& data) {
            data["slot"] = slot;
            JsonObject presetObj = data["preset"].to<JsonObject>();
            buildPresetJson(savedPreset, slot, presetObj);
        });
    client->text(response);

    // Broadcast to all clients
    if (ctx.ws) {
        JsonDocument broadcastDoc;
        broadcastDoc["type"] = "effectPresets.saved";
        broadcastDoc["slot"] = slot;
        JsonObject presetObj = broadcastDoc["preset"].to<JsonObject>();
        buildPresetJson(savedPreset, slot, presetObj);

        String broadcastMsg;
        serializeJson(broadcastDoc, broadcastMsg);
        ctx.ws->textAll(broadcastMsg);
    }
}

// ============================================================================
// Command: effectPresets.load
// ============================================================================

/**
 * @brief Load and apply a preset to the renderer
 *
 * Request:  {"type": "effectPresets.load", "id": 0, "requestId": "..."}
 * Response: {"type": "effectPresets.loaded", "data": {"preset": {...}}}
 */
static void handleEffectPresetsLoad(AsyncWebSocketClient* client,
                                    JsonDocument& doc,
                                    const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    // Validate required parameter
    if (!doc.containsKey("id")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD,
                                  "Missing required field: id", requestId));
        return;
    }

    uint8_t slot = doc["id"] | 255;
    if (slot >= persistence::EffectPresetManager::MAX_PRESETS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE,
                                  "Slot id out of range (0-15)", requestId));
        return;
    }

    // Check system availability
    if (!ctx.renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY,
                                  "Renderer not available", requestId));
        return;
    }

    auto& mgr = persistence::EffectPresetManager::instance();
    if (!mgr.isInitialised()) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                  "EffectPresetManager not initialised", requestId));
        return;
    }

    // Load the preset
    persistence::EffectPreset preset;
    persistence::NVSResult result = mgr.load(slot, preset);

    if (result == persistence::NVSResult::NOT_FOUND) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND,
                                  "Preset slot is empty", requestId));
        return;
    }

    if (result != persistence::NVSResult::OK) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                  "Failed to load preset", requestId));
        return;
    }

    // Apply preset to renderer via ActorSystem
    ctx.actorSystem.setEffect(preset.effectId);
    ctx.actorSystem.setPalette(preset.paletteId);
    ctx.actorSystem.setBrightness(preset.brightness);
    ctx.actorSystem.setSpeed(preset.speed);
    ctx.actorSystem.setHue(preset.hue);
    ctx.actorSystem.setSaturation(preset.saturation);
    ctx.actorSystem.setIntensity(preset.intensity);
    ctx.actorSystem.setComplexity(preset.complexity);
    ctx.actorSystem.setVariation(preset.variation);

    // Broadcast status update
    if (ctx.broadcastStatus) {
        ctx.broadcastStatus();
    }

    // Send response
    String response = buildWsResponse("effectPresets.loaded", requestId,
        [&preset, slot](JsonObject& data) {
            JsonObject presetObj = data["preset"].to<JsonObject>();
            buildPresetJson(preset, slot, presetObj);
        });
    client->text(response);
}

// ============================================================================
// Command: effectPresets.delete
// ============================================================================

/**
 * @brief Delete a preset from a slot
 *
 * Request:  {"type": "effectPresets.delete", "id": 0, "requestId": "..."}
 * Response: {"type": "effectPresets.deleted", "data": {"id": 0}}
 * Broadcast: {"type": "effectPresets.deleted", "id": 0}
 */
static void handleEffectPresetsDelete(AsyncWebSocketClient* client,
                                      JsonDocument& doc,
                                      const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    // Validate required parameter
    if (!doc.containsKey("id")) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD,
                                  "Missing required field: id", requestId));
        return;
    }

    uint8_t slot = doc["id"] | 255;
    if (slot >= persistence::EffectPresetManager::MAX_PRESETS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE,
                                  "Slot id out of range (0-15)", requestId));
        return;
    }

    auto& mgr = persistence::EffectPresetManager::instance();
    if (!mgr.isInitialised()) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                  "EffectPresetManager not initialised", requestId));
        return;
    }

    // Delete the preset
    persistence::NVSResult result = mgr.remove(slot);

    if (result == persistence::NVSResult::NOT_FOUND) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND,
                                  "Preset slot is already empty", requestId));
        return;
    }

    if (result != persistence::NVSResult::OK) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR,
                                  "Failed to delete preset", requestId));
        return;
    }

    // Send response to requesting client
    String response = buildWsResponse("effectPresets.deleted", requestId,
        [slot](JsonObject& data) {
            data["id"] = slot;
        });
    client->text(response);

    // Broadcast to all clients
    if (ctx.ws) {
        JsonDocument broadcastDoc;
        broadcastDoc["type"] = "effectPresets.deleted";
        broadcastDoc["id"] = slot;

        String broadcastMsg;
        serializeJson(broadcastDoc, broadcastMsg);
        ctx.ws->textAll(broadcastMsg);
    }
}

// ============================================================================
// Registration
// ============================================================================

void registerWsEffectPresetCommands(const WebServerContext& ctx) {
    (void)ctx;  // Context captured by handlers at call time

    WsCommandRouter::registerCommand("effectPresets.list", handleEffectPresetsList);
    WsCommandRouter::registerCommand("effectPresets.get", handleEffectPresetsGet);
    WsCommandRouter::registerCommand("effectPresets.saveCurrent", handleEffectPresetsSaveCurrent);
    WsCommandRouter::registerCommand("effectPresets.load", handleEffectPresetsLoad);
    WsCommandRouter::registerCommand("effectPresets.delete", handleEffectPresetsDelete);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
