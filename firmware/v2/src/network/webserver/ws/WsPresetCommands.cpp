/**
 * @file WsPresetCommands.cpp
 * @brief WebSocket effect preset command handlers implementation
 *
 * LightwaveOS v2 - WebSocket API
 *
 * Implements effect preset CRUD operations over WebSocket.
 */

#include "WsPresetCommands.h"
#include "../WsCommandRouter.h"
#include "../WebServerContext.h"
#include "../../ApiResponse.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../core/persistence/EffectPreset.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace ws {

using namespace lightwaveos::persistence;

/**
 * @brief Handle effect.presets.list command
 *
 * Lists all saved effect presets with their names and IDs.
 *
 * Request: {"type": "effect.presets.list", "requestId": "..."}
 * Response: {"type": "effect.presets.list", "success": true, "data": {
 *   "count": 3, "maxPresets": 10, "presets": [{"id": 0, "name": "..."}, ...]
 * }}
 */
static void handlePresetsList(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    auto& mgr = EffectPresetManager::instance();

    char names[EffectPresetManager::MAX_PRESETS][EffectPreset::NAME_MAX_LEN];
    uint8_t ids[EffectPresetManager::MAX_PRESETS];
    uint8_t count = mgr.listPresets(names, ids);

    String response = buildWsResponse("effect.presets.list", requestId, [count, &names, &ids](JsonObject& data) {
        data["count"] = count;
        data["maxPresets"] = EffectPresetManager::MAX_PRESETS;
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
 * @brief Handle effect.presets.get command
 *
 * Gets details of a specific preset by ID.
 *
 * Request: {"type": "effect.presets.get", "id": 0, "requestId": "..."}
 * Response: {"type": "effect.presets.get", "success": true, "data": {
 *   "id": 0, "name": "...", "effectId": 5, "brightness": 200, "speed": 25, "paletteId": 3
 * }}
 */
static void handlePresetsGet(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint8_t presetId = doc["id"] | 255;

    if (presetId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "id required", requestId));
        return;
    }

    if (presetId >= EffectPresetManager::MAX_PRESETS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-9", requestId));
        return;
    }

    auto& mgr = EffectPresetManager::instance();
    EffectPreset preset;
    if (!mgr.getPreset(presetId, preset)) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND, "Preset not found", requestId));
        return;
    }

    String response = buildWsResponse("effect.presets.get", requestId, [presetId, &preset](JsonObject& data) {
        data["id"] = presetId;
        data["name"] = preset.name;
        data["effectId"] = preset.effectId;
        data["brightness"] = preset.brightness;
        data["speed"] = preset.speed;
        data["paletteId"] = preset.paletteId;
    });
    client->text(response);
}

/**
 * @brief Handle effect.presets.save command
 *
 * Saves current effect configuration as a new preset.
 * Uses current renderer state for effect parameters, unless overridden in request.
 *
 * Request: {"type": "effect.presets.save", "name": "My Preset", "requestId": "..."}
 *   Optional overrides: effectId, brightness, speed, paletteId
 * Response: {"type": "effect.presets.save", "success": true, "data": {
 *   "id": 0, "name": "My Preset", "effectId": 5, "brightness": 200, "speed": 25, "paletteId": 3
 * }}
 */
static void handlePresetsSave(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";

    if (!ctx.renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Renderer not available", requestId));
        return;
    }

    // Get preset name (required)
    const char* name = doc["name"] | "Unnamed";

    // Get current effect state from renderer
    uint8_t effectId = ctx.renderer->getCurrentEffect();
    uint8_t brightness = ctx.renderer->getBrightness();
    uint8_t speed = ctx.renderer->getSpeed();
    uint8_t paletteId = ctx.renderer->getPaletteIndex();

    // Allow overriding values in the request
    if (doc.containsKey("effectId")) effectId = doc["effectId"].as<uint8_t>();
    if (doc.containsKey("brightness")) brightness = doc["brightness"].as<uint8_t>();
    if (doc.containsKey("speed")) speed = doc["speed"].as<uint8_t>();
    if (doc.containsKey("paletteId")) paletteId = doc["paletteId"].as<uint8_t>();

    // Save preset
    auto& mgr = EffectPresetManager::instance();
    int8_t slotId = mgr.savePreset(name, effectId, brightness, speed, paletteId);

    if (slotId < 0) {
        client->text(buildWsError(ErrorCodes::STORAGE_FULL, "No free preset slots", requestId));
        return;
    }

    String response = buildWsResponse("effect.presets.save", requestId,
        [slotId, name, effectId, brightness, speed, paletteId](JsonObject& data) {
            data["id"] = slotId;
            data["name"] = name;
            data["effectId"] = effectId;
            data["brightness"] = brightness;
            data["speed"] = speed;
            data["paletteId"] = paletteId;
            data["message"] = "Preset saved";
        });
    client->text(response);
}

/**
 * @brief Handle effect.presets.apply command
 *
 * Applies a saved preset, setting effect, brightness, speed, and palette.
 *
 * Request: {"type": "effect.presets.apply", "id": 0, "requestId": "..."}
 * Response: {"type": "effect.presets.apply", "success": true, "data": {
 *   "id": 0, "name": "...", "effectId": 5, "brightness": 200, "speed": 25, "paletteId": 3
 * }}
 */
static void handlePresetsApply(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint8_t presetId = doc["id"] | 255;

    if (presetId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "id required", requestId));
        return;
    }

    if (!ctx.renderer) {
        client->text(buildWsError(ErrorCodes::SYSTEM_NOT_READY, "Renderer not available", requestId));
        return;
    }

    if (presetId >= EffectPresetManager::MAX_PRESETS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-9", requestId));
        return;
    }

    auto& mgr = EffectPresetManager::instance();
    EffectPreset preset;
    if (!mgr.getPreset(presetId, preset)) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND, "Preset not found", requestId));
        return;
    }

    // Apply preset values via ActorSystem
    ctx.actorSystem.setEffect(preset.effectId);
    ctx.actorSystem.setBrightness(preset.brightness);
    ctx.actorSystem.setSpeed(preset.speed);
    ctx.actorSystem.setPalette(preset.paletteId);

    // Broadcast status update
    if (ctx.broadcastStatus) ctx.broadcastStatus();

    String response = buildWsResponse("effect.presets.apply", requestId, [presetId, &preset](JsonObject& data) {
        data["id"] = presetId;
        data["name"] = preset.name;
        data["effectId"] = preset.effectId;
        data["brightness"] = preset.brightness;
        data["speed"] = preset.speed;
        data["paletteId"] = preset.paletteId;
        data["message"] = "Preset applied";
    });
    client->text(response);
}

/**
 * @brief Handle effect.presets.delete command
 *
 * Deletes a saved preset by ID.
 *
 * Request: {"type": "effect.presets.delete", "id": 0, "requestId": "..."}
 * Response: {"type": "effect.presets.delete", "success": true, "data": {"id": 0, "message": "Preset deleted"}}
 */
static void handlePresetsDelete(AsyncWebSocketClient* client, JsonDocument& doc, const WebServerContext& ctx) {
    const char* requestId = doc["requestId"] | "";
    uint8_t presetId = doc["id"] | 255;

    if (presetId == 255) {
        client->text(buildWsError(ErrorCodes::MISSING_FIELD, "id required", requestId));
        return;
    }

    if (presetId >= EffectPresetManager::MAX_PRESETS) {
        client->text(buildWsError(ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-9", requestId));
        return;
    }

    auto& mgr = EffectPresetManager::instance();
    if (!mgr.hasPreset(presetId)) {
        client->text(buildWsError(ErrorCodes::NOT_FOUND, "Preset not found", requestId));
        return;
    }

    if (!mgr.deletePreset(presetId)) {
        client->text(buildWsError(ErrorCodes::INTERNAL_ERROR, "Failed to delete preset", requestId));
        return;
    }

    String response = buildWsResponse("effect.presets.delete", requestId, [presetId](JsonObject& data) {
        data["id"] = presetId;
        data["message"] = "Preset deleted";
    });
    client->text(response);
}

void registerWsPresetCommands(const WebServerContext& ctx) {
    WsCommandRouter::registerCommand("effect.presets.list", handlePresetsList);
    WsCommandRouter::registerCommand("effect.presets.get", handlePresetsGet);
    WsCommandRouter::registerCommand("effect.presets.save", handlePresetsSave);
    WsCommandRouter::registerCommand("effect.presets.apply", handlePresetsApply);
    WsCommandRouter::registerCommand("effect.presets.delete", handlePresetsDelete);
}

} // namespace ws
} // namespace webserver
} // namespace network
} // namespace lightwaveos
