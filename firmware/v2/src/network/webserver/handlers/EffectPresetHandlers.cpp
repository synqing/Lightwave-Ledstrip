/**
 * @file EffectPresetHandlers.cpp
 * @brief Effect preset HTTP handlers implementation
 */

#include "EffectPresetHandlers.h"
#include "../../ApiResponse.h"
#include "../../RequestValidator.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../core/persistence/EffectPreset.h"
#include <cstring>

#define LW_LOG_TAG "EffectPresetHandlers"
#include "../../../utils/Log.h"

using namespace lightwaveos::actors;
using namespace lightwaveos::persistence;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void EffectPresetHandlers::handleList(AsyncWebServerRequest* request) {
    auto& mgr = EffectPresetManager::instance();

    char names[EffectPresetManager::MAX_PRESETS][EffectPreset::NAME_MAX_LEN];
    uint8_t ids[EffectPresetManager::MAX_PRESETS];
    uint8_t count = mgr.listPresets(names, ids);

    sendSuccessResponse(request, [count, &names, &ids](JsonObject& data) {
        data["count"] = count;
        data["maxPresets"] = EffectPresetManager::MAX_PRESETS;
        JsonArray presets = data["presets"].to<JsonArray>();
        for (uint8_t i = 0; i < count; i++) {
            JsonObject p = presets.add<JsonObject>();
            p["id"] = ids[i];
            p["name"] = names[i];
        }
    });
}

void EffectPresetHandlers::handleGet(AsyncWebServerRequest* request, uint8_t presetId) {
    auto& mgr = EffectPresetManager::instance();

    if (presetId >= EffectPresetManager::MAX_PRESETS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-9", "id");
        return;
    }

    EffectPreset preset;
    if (!mgr.getPreset(presetId, preset)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found", "id");
        return;
    }

    sendSuccessResponse(request, [presetId, &preset](JsonObject& data) {
        data["id"] = presetId;
        data["name"] = preset.name;
        data["effectId"] = preset.effectId;
        data["brightness"] = preset.brightness;
        data["speed"] = preset.speed;
        data["paletteId"] = preset.paletteId;
    });
}

void EffectPresetHandlers::handleSave(AsyncWebServerRequest* request,
                                       uint8_t* data, size_t len,
                                       RendererActor* renderer) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    // Parse JSON body
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON");
        return;
    }

    // Get preset name (required)
    const char* name = doc["name"] | "Unnamed";

    // Get current effect state from renderer
    uint8_t effectId = renderer->getCurrentEffect();
    uint8_t brightness = renderer->getBrightness();
    uint8_t speed = renderer->getSpeed();
    uint8_t paletteId = renderer->getPaletteIndex();

    // Allow overriding values in the request
    if (doc.containsKey("effectId")) effectId = doc["effectId"].as<uint8_t>();
    if (doc.containsKey("brightness")) brightness = doc["brightness"].as<uint8_t>();
    if (doc.containsKey("speed")) speed = doc["speed"].as<uint8_t>();
    if (doc.containsKey("paletteId")) paletteId = doc["paletteId"].as<uint8_t>();

    // Save preset
    auto& mgr = EffectPresetManager::instance();
    int8_t slotId = mgr.savePreset(name, effectId, brightness, speed, paletteId);

    if (slotId < 0) {
        sendErrorResponse(request, HttpStatus::INSUFFICIENT_STORAGE,
                          ErrorCodes::STORAGE_FULL, "No free preset slots");
        return;
    }

    sendSuccessResponse(request, [slotId, name, effectId, brightness, speed, paletteId](JsonObject& d) {
        d["id"] = slotId;
        d["name"] = name;
        d["effectId"] = effectId;
        d["brightness"] = brightness;
        d["speed"] = speed;
        d["paletteId"] = paletteId;
        d["message"] = "Preset saved";
    });
}

void EffectPresetHandlers::handleApply(AsyncWebServerRequest* request, uint8_t presetId,
                                        ActorSystem& actorSystem,
                                        RendererActor* renderer) {
    if (!renderer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY, "Renderer not available");
        return;
    }

    auto& mgr = EffectPresetManager::instance();

    if (presetId >= EffectPresetManager::MAX_PRESETS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-9", "id");
        return;
    }

    EffectPreset preset;
    if (!mgr.getPreset(presetId, preset)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found", "id");
        return;
    }

    // Apply preset values via ActorSystem
    actorSystem.setEffect(preset.effectId);
    actorSystem.setBrightness(preset.brightness);
    actorSystem.setSpeed(preset.speed);
    actorSystem.setPalette(preset.paletteId);

    LW_LOGI("Applied preset '%s' (id=%d): effect=%d, brightness=%d, speed=%d, palette=%d",
            preset.name, presetId, preset.effectId, preset.brightness, preset.speed, preset.paletteId);

    sendSuccessResponse(request, [presetId, &preset](JsonObject& d) {
        d["id"] = presetId;
        d["name"] = preset.name;
        d["effectId"] = preset.effectId;
        d["brightness"] = preset.brightness;
        d["speed"] = preset.speed;
        d["paletteId"] = preset.paletteId;
        d["message"] = "Preset applied";
    });
}

void EffectPresetHandlers::handleDelete(AsyncWebServerRequest* request, uint8_t presetId) {
    auto& mgr = EffectPresetManager::instance();

    if (presetId >= EffectPresetManager::MAX_PRESETS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-9", "id");
        return;
    }

    if (!mgr.hasPreset(presetId)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found", "id");
        return;
    }

    if (!mgr.deletePreset(presetId)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to delete preset");
        return;
    }

    sendSuccessResponse(request, [presetId](JsonObject& d) {
        d["id"] = presetId;
        d["message"] = "Preset deleted";
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
