/**
 * @file EffectPresetHandlers.cpp
 * @brief Effect preset REST API handlers
 *
 * Implements full CRUD operations for effect presets using the
 * EffectPresetManager persistence layer and ActorSystem messaging.
 *
 * Endpoints:
 *   GET  /api/v1/effect-presets           - List all presets with metadata
 *   GET  /api/v1/effect-presets/get?id=N  - Get full preset details by slot ID
 *   POST /api/v1/effect-presets           - Save current effect as new preset
 *   POST /api/v1/effect-presets/apply?id=N - Apply preset to renderer
 *   DELETE /api/v1/effect-presets/delete?id=N - Delete preset by ID
 */

#include "EffectPresetHandlers.h"
#include "../../ApiResponse.h"
#include "../../../core/persistence/EffectPresetManager.h"
#include "../../../core/actors/ActorSystem.h"
#include <ArduinoJson.h>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

using namespace persistence;

// ============================================================================
// handleList - GET /api/v1/effect-presets
// ============================================================================

void EffectPresetHandlers::handleList(AsyncWebServerRequest* request) {
    auto& mgr = EffectPresetManager::instance();

    // Ensure manager is initialised
    if (!mgr.isInitialised()) {
        if (!mgr.init()) {
            sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                              ErrorCodes::INTERNAL_ERROR,
                              "Effect preset manager failed to initialise");
            return;
        }
    }

    EffectPresetMetadata metadata[EffectPresetManager::MAX_PRESETS];
    uint8_t count = 0;

    NVSResult result = mgr.list(metadata, count);
    if (result != NVSResult::OK) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR,
                          "Failed to list effect presets");
        return;
    }

    sendSuccessResponseLarge(request, [&metadata, count](JsonObject& data) {
        JsonArray presets = data["presets"].to<JsonArray>();

        for (uint8_t i = 0; i < EffectPresetManager::MAX_PRESETS; i++) {
            if (metadata[i].occupied) {
                JsonObject p = presets.add<JsonObject>();
                p["id"] = metadata[i].slot;
                p["name"] = metadata[i].name;
                p["effectId"] = metadata[i].effectId;
                p["paletteId"] = metadata[i].paletteId;
                p["timestamp"] = metadata[i].timestamp;
            }
        }

        data["count"] = count;
    }, 1024);
}

// ============================================================================
// handleGet - GET /api/v1/effect-presets/get?id=N
// ============================================================================

void EffectPresetHandlers::handleGet(AsyncWebServerRequest* request, uint8_t id) {
    auto& mgr = EffectPresetManager::instance();

    // Validate slot ID
    if (id >= EffectPresetManager::MAX_PRESETS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE,
                          "Preset ID must be 0-15", "id");
        return;
    }

    // Ensure manager is initialised
    if (!mgr.isInitialised()) {
        if (!mgr.init()) {
            sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                              ErrorCodes::INTERNAL_ERROR,
                              "Effect preset manager failed to initialise");
            return;
        }
    }

    EffectPreset preset;
    NVSResult result = mgr.load(id, preset);

    if (result == NVSResult::NOT_FOUND) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND,
                          "Preset not found", "id");
        return;
    }

    if (result == NVSResult::CHECKSUM_ERROR) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR,
                          "Preset data corrupted");
        return;
    }

    if (result != NVSResult::OK) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR,
                          "Failed to load preset");
        return;
    }

    sendSuccessResponse(request, [&preset, id](JsonObject& data) {
        data["id"] = id;
        data["name"] = preset.name;
        data["effectId"] = preset.effectId;
        data["paletteId"] = preset.paletteId;
        data["brightness"] = preset.brightness;
        data["speed"] = preset.speed;
        data["timestamp"] = preset.timestamp;

        // Expression/semantic parameters
        JsonObject params = data["parameters"].to<JsonObject>();
        params["mood"] = preset.mood;
        params["trails"] = preset.trails;
        params["hue"] = preset.hue;
        params["saturation"] = preset.saturation;
        params["intensity"] = preset.intensity;
        params["complexity"] = preset.complexity;
        params["variation"] = preset.variation;
    });
}

// ============================================================================
// handleSave - POST /api/v1/effect-presets
// ============================================================================

void EffectPresetHandlers::handleSave(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                                       actors::RendererActor* renderer) {
    auto& mgr = EffectPresetManager::instance();

    // Parse JSON body
    JsonDocument doc;
    DeserializationError jsonErr = deserializeJson(doc, data, len);
    if (jsonErr) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON,
                          "JSON parse error");
        return;
    }

    // Get preset name (required)
    const char* name = doc["name"] | nullptr;
    if (name == nullptr || strlen(name) == 0) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::MISSING_FIELD,
                          "Missing required field: name", "name");
        return;
    }

    // Truncate name if too long
    char safeName[EffectPreset::NAME_MAX_LEN];
    strncpy(safeName, name, EffectPreset::NAME_MAX_LEN - 1);
    safeName[EffectPreset::NAME_MAX_LEN - 1] = '\0';

    // Check if renderer is available
    if (renderer == nullptr) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY,
                          "Renderer not available");
        return;
    }

    // Ensure manager is initialised
    if (!mgr.isInitialised()) {
        if (!mgr.init()) {
            sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                              ErrorCodes::INTERNAL_ERROR,
                              "Effect preset manager failed to initialise");
            return;
        }
    }

    // Find a free slot or use specified slot
    int8_t slot = -1;
    if (doc.containsKey("slot")) {
        uint8_t requestedSlot = doc["slot"].as<uint8_t>();
        if (requestedSlot >= EffectPresetManager::MAX_PRESETS) {
            sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                              ErrorCodes::OUT_OF_RANGE,
                              "Slot must be 0-15", "slot");
            return;
        }
        slot = requestedSlot;
    } else {
        slot = mgr.findFreeSlot();
        if (slot < 0) {
            sendErrorResponse(request, HttpStatus::INSUFFICIENT_STORAGE,
                              ErrorCodes::STORAGE_FULL,
                              "No free preset slots available");
            return;
        }
    }

    // Save current effect state from renderer
    NVSResult result = mgr.saveCurrentEffect(static_cast<uint8_t>(slot), safeName, renderer);

    if (result != NVSResult::OK) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::OPERATION_FAILED,
                          "Failed to save preset");
        return;
    }

    // Return success with saved preset details
    sendSuccessResponse(request, [slot, safeName, renderer](JsonObject& respData) {
        respData["id"] = static_cast<uint8_t>(slot);
        respData["name"] = safeName;
        respData["effectId"] = renderer->getCurrentEffect();
        respData["paletteId"] = renderer->getPaletteIndex();
        respData["message"] = "Preset saved";
    }, HttpStatus::CREATED);
}

// ============================================================================
// handleApply - POST /api/v1/effect-presets/apply?id=N
// ============================================================================

void EffectPresetHandlers::handleApply(AsyncWebServerRequest* request, uint8_t id,
                                        actors::ActorSystem& orchestrator,
                                        actors::RendererActor* renderer) {
    auto& mgr = EffectPresetManager::instance();

    // Validate slot ID
    if (id >= EffectPresetManager::MAX_PRESETS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE,
                          "Preset ID must be 0-15", "id");
        return;
    }

    // Ensure manager is initialised
    if (!mgr.isInitialised()) {
        if (!mgr.init()) {
            sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                              ErrorCodes::INTERNAL_ERROR,
                              "Effect preset manager failed to initialise");
            return;
        }
    }

    // Load preset from NVS
    EffectPreset preset;
    NVSResult result = mgr.load(id, preset);

    if (result == NVSResult::NOT_FOUND) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND,
                          "Preset not found", "id");
        return;
    }

    if (result == NVSResult::CHECKSUM_ERROR) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR,
                          "Preset data corrupted");
        return;
    }

    if (result != NVSResult::OK) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR,
                          "Failed to load preset");
        return;
    }

    // Check if orchestrator is running
    if (!orchestrator.isRunning()) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::SYSTEM_NOT_READY,
                          "Actor system not running");
        return;
    }

    // Apply preset via ActorSystem messages (thread-safe)
    // Apply in order: effect first, then parameters
    bool success = true;

    success = success && orchestrator.setEffect(preset.effectId);
    success = success && orchestrator.setPalette(preset.paletteId);
    success = success && orchestrator.setBrightness(preset.brightness);
    success = success && orchestrator.setSpeed(preset.speed);
    success = success && orchestrator.setHue(preset.hue);
    success = success && orchestrator.setIntensity(preset.intensity);
    success = success && orchestrator.setSaturation(preset.saturation);
    success = success && orchestrator.setComplexity(preset.complexity);
    success = success && orchestrator.setVariation(preset.variation);
    success = success && orchestrator.setMood(preset.mood);
    success = success && orchestrator.setFadeAmount(preset.trails);

    if (!success) {
        // Partial application may have occurred, but we report the failure
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::OPERATION_FAILED,
                          "Failed to apply all preset parameters (queue may be full)");
        return;
    }

    sendSuccessResponse(request, [id, &preset](JsonObject& respData) {
        respData["id"] = id;
        respData["name"] = preset.name;
        respData["effectId"] = preset.effectId;
        respData["paletteId"] = preset.paletteId;
        respData["message"] = "Preset applied";
    });
}

// ============================================================================
// handleDelete - DELETE /api/v1/effect-presets/delete?id=N
// ============================================================================

void EffectPresetHandlers::handleDelete(AsyncWebServerRequest* request, uint8_t id) {
    auto& mgr = EffectPresetManager::instance();

    // Validate slot ID
    if (id >= EffectPresetManager::MAX_PRESETS) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::OUT_OF_RANGE,
                          "Preset ID must be 0-15", "id");
        return;
    }

    // Ensure manager is initialised
    if (!mgr.isInitialised()) {
        if (!mgr.init()) {
            sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                              ErrorCodes::INTERNAL_ERROR,
                              "Effect preset manager failed to initialise");
            return;
        }
    }

    // Check if preset exists
    if (!mgr.isSlotOccupied(id)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND,
                          "Preset not found", "id");
        return;
    }

    // Delete the preset
    NVSResult result = mgr.remove(id);

    if (result != NVSResult::OK) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::OPERATION_FAILED,
                          "Failed to delete preset");
        return;
    }

    sendSuccessResponse(request, [id](JsonObject& respData) {
        respData["id"] = id;
        respData["message"] = "Preset deleted";
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
