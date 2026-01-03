/**
 * @file ZonePresetHandlers.cpp
 * @brief Zone preset HTTP handlers implementation
 */

#include "ZonePresetHandlers.h"
#include "../../ApiResponse.h"
#include "../../RequestValidator.h"
#include "../../../core/actors/ActorSystem.h"
#include "../../../core/actors/RendererActor.h"
#include "../../../core/persistence/ZonePreset.h"
#include "../../../effects/zones/ZoneComposer.h"
#include "../../../effects/zones/BlendMode.h"
#include <cstring>

#define LW_LOG_TAG "ZonePresetHandlers"
#include "../../../utils/Log.h"

using namespace lightwaveos::actors;
using namespace lightwaveos::persistence;
using namespace lightwaveos::zones;

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

void ZonePresetHandlers::handleList(AsyncWebServerRequest* request) {
    auto& mgr = ZonePresetManager::instance();

    char names[ZonePresetManager::MAX_PRESETS][ZonePreset::NAME_MAX_LEN];
    uint8_t ids[ZonePresetManager::MAX_PRESETS];
    uint8_t count = mgr.listPresets(names, ids);

    sendSuccessResponse(request, [count, &names, &ids](JsonObject& data) {
        data["count"] = count;
        data["maxPresets"] = ZonePresetManager::MAX_PRESETS;
        JsonArray presets = data["presets"].to<JsonArray>();
        for (uint8_t i = 0; i < count; i++) {
            JsonObject p = presets.add<JsonObject>();
            p["id"] = ids[i];
            p["name"] = names[i];
        }
    });
}

void ZonePresetHandlers::handleGet(AsyncWebServerRequest* request, uint8_t presetId) {
    auto& mgr = ZonePresetManager::instance();

    if (presetId >= ZonePresetManager::MAX_PRESETS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-4", "id");
        return;
    }

    ZonePreset preset;
    if (!mgr.getPreset(presetId, preset)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found", "id");
        return;
    }

    sendSuccessResponse(request, [presetId, &preset](JsonObject& data) {
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
}

void ZonePresetHandlers::handleSave(AsyncWebServerRequest* request,
                                     uint8_t* data, size_t len,
                                     ZoneComposer* zoneComposer) {
    if (!zoneComposer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
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

    // Check if saving to specific slot
    int8_t slotId = -1;
    if (doc.containsKey("id")) {
        uint8_t requestedId = doc["id"].as<uint8_t>();
        if (requestedId < ZonePresetManager::MAX_PRESETS) {
            // Save to specific slot (overwrite)
            auto& mgr = ZonePresetManager::instance();
            if (mgr.savePresetAt(requestedId, name, zoneComposer)) {
                slotId = requestedId;
            }
        } else {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-4", "id");
            return;
        }
    } else {
        // Find free slot
        auto& mgr = ZonePresetManager::instance();
        slotId = mgr.savePreset(name, zoneComposer);
    }

    if (slotId < 0) {
        sendErrorResponse(request, HttpStatus::INSUFFICIENT_STORAGE,
                          ErrorCodes::STORAGE_FULL, "No free preset slots");
        return;
    }

    // Get saved preset for response
    ZonePreset savedPreset;
    ZonePresetManager::instance().getPreset(slotId, savedPreset);

    sendSuccessResponse(request, [slotId, &savedPreset](JsonObject& d) {
        d["id"] = slotId;
        d["name"] = savedPreset.name;
        d["zoneCount"] = savedPreset.zoneCount;
        d["zonesEnabled"] = savedPreset.zonesEnabled;
        d["message"] = "Zone preset saved";
    });
}

void ZonePresetHandlers::handleApply(AsyncWebServerRequest* request, uint8_t presetId,
                                      ActorSystem& actorSystem,
                                      ZoneComposer* zoneComposer,
                                      std::function<void()> broadcastZoneState) {
    if (!zoneComposer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    auto& mgr = ZonePresetManager::instance();

    if (presetId >= ZonePresetManager::MAX_PRESETS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-4", "id");
        return;
    }

    ZonePreset preset;
    if (!mgr.getPreset(presetId, preset)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found", "id");
        return;
    }

    // Apply the preset
    if (!mgr.applyPreset(presetId, zoneComposer, actorSystem)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to apply preset");
        return;
    }

    // Broadcast zone state change
    if (broadcastZoneState) {
        broadcastZoneState();
    }

    LW_LOGI("Applied zone preset '%s' (id=%d): zones=%d, enabled=%s",
            preset.name, presetId, preset.zoneCount, preset.zonesEnabled ? "true" : "false");

    sendSuccessResponse(request, [presetId, &preset](JsonObject& d) {
        d["id"] = presetId;
        d["name"] = preset.name;
        d["zoneCount"] = preset.zoneCount;
        d["zonesEnabled"] = preset.zonesEnabled;
        d["message"] = "Zone preset applied";
    });
}

void ZonePresetHandlers::handleDelete(AsyncWebServerRequest* request, uint8_t presetId) {
    auto& mgr = ZonePresetManager::instance();

    if (presetId >= ZonePresetManager::MAX_PRESETS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE, "Preset ID must be 0-4", "id");
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
        d["message"] = "Zone preset deleted";
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
