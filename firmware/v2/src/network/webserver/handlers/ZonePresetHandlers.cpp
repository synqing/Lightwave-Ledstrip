/**
 * @file ZonePresetHandlers.cpp
 * @brief Zone preset REST API handlers
 *
 * Provides endpoints for zone preset management:
 * - Built-in presets (IDs 0-4): Read-only, defined in ZoneConfigManager
 * - User presets (IDs 10-19): Stored in NVS, user-manageable
 *
 * LightwaveOS v2 - Network API
 */

#include "ZonePresetHandlers.h"
#include "../../ApiResponse.h"
#include "../../../core/persistence/ZoneConfigManager.h"
#include "../../../core/persistence/NVSManager.h"
#include "../../../effects/zones/ZoneComposer.h"
#include "../../../effects/zones/ZoneDefinition.h"
#include "../../../effects/zones/BlendMode.h"
#include <ArduinoJson.h>
#include <cstring>

namespace lightwaveos {
namespace network {
namespace webserver {
namespace handlers {

// ============================================================================
// User Preset Storage Configuration
// ============================================================================

namespace {

// NVS namespace and key prefix for user presets
constexpr const char* NVS_NAMESPACE = "zone_presets";
constexpr const char* NVS_KEY_COUNT = "count";
constexpr const char* NVS_KEY_PREFIX = "preset_";

// Built-in preset ID range: 0-4
constexpr uint8_t BUILTIN_PRESET_COUNT = persistence::ZONE_PRESET_COUNT;

// User preset ID range: 10-19
constexpr uint8_t USER_PRESET_ID_START = 10;
constexpr uint8_t USER_PRESET_MAX_COUNT = 10;

// User preset structure for NVS storage
struct UserZonePreset {
    char name[32];                      // Preset name
    uint32_t timestamp;                 // Creation/modification timestamp
    persistence::ZoneConfigData config; // Zone configuration
    uint32_t checksum;                  // CRC32 for validation

    void calculateChecksum() {
        const size_t dataSize = offsetof(UserZonePreset, checksum);
        checksum = persistence::NVSManager::calculateCRC32(this, dataSize);
    }

    bool isValid() const {
        const size_t dataSize = offsetof(UserZonePreset, checksum);
        uint32_t calculated = persistence::NVSManager::calculateCRC32(this, dataSize);
        return (checksum == calculated);
    }
};

/**
 * @brief Get NVS key for a user preset slot
 */
String getUserPresetKey(uint8_t slot) {
    char key[16];
    snprintf(key, sizeof(key), "%s%u", NVS_KEY_PREFIX, slot);
    return String(key);
}

/**
 * @brief Load user preset count from NVS
 */
uint8_t loadUserPresetCount() {
    return NVS_MANAGER.loadUint8(NVS_NAMESPACE, NVS_KEY_COUNT, 0);
}

/**
 * @brief Save user preset count to NVS
 */
void saveUserPresetCount(uint8_t count) {
    NVS_MANAGER.saveUint8(NVS_NAMESPACE, NVS_KEY_COUNT, count);
}

/**
 * @brief Load a user preset from NVS
 * @param slot User preset slot (0-9)
 * @param preset Output: loaded preset data
 * @return true if loaded successfully
 */
bool loadUserPreset(uint8_t slot, UserZonePreset& preset) {
    if (slot >= USER_PRESET_MAX_COUNT) return false;

    String key = getUserPresetKey(slot);
    auto result = NVS_MANAGER.loadBlob(
        NVS_NAMESPACE, key.c_str(), &preset, sizeof(preset));

    if (result != persistence::NVSResult::OK) return false;
    if (!preset.isValid()) return false;

    return true;
}

/**
 * @brief Save a user preset to NVS
 * @param slot User preset slot (0-9)
 * @param preset Preset data to save
 * @return true if saved successfully
 */
bool saveUserPreset(uint8_t slot, UserZonePreset& preset) {
    if (slot >= USER_PRESET_MAX_COUNT) return false;

    preset.calculateChecksum();
    String key = getUserPresetKey(slot);

    auto result = NVS_MANAGER.saveBlob(
        NVS_NAMESPACE, key.c_str(), &preset, sizeof(preset));

    return (result == persistence::NVSResult::OK);
}

/**
 * @brief Delete a user preset from NVS
 * @param slot User preset slot (0-9)
 * @return true if deleted successfully
 */
bool deleteUserPreset(uint8_t slot) {
    if (slot >= USER_PRESET_MAX_COUNT) return false;

    String key = getUserPresetKey(slot);
    auto result = NVS_MANAGER.eraseKey(NVS_NAMESPACE, key.c_str());

    return (result == persistence::NVSResult::OK);
}

/**
 * @brief Find next available user preset slot
 * @return Slot number (0-9) or 255 if full
 */
uint8_t findNextUserSlot() {
    UserZonePreset preset;
    for (uint8_t i = 0; i < USER_PRESET_MAX_COUNT; i++) {
        if (!loadUserPreset(i, preset)) {
            return i;
        }
    }
    return 255; // No slot available
}

/**
 * @brief Check if preset ID is a built-in preset
 */
bool isBuiltinPreset(uint8_t id) {
    return id < BUILTIN_PRESET_COUNT;
}

/**
 * @brief Convert user preset ID to slot index
 */
uint8_t userIdToSlot(uint8_t id) {
    if (id >= USER_PRESET_ID_START && id < USER_PRESET_ID_START + USER_PRESET_MAX_COUNT) {
        return id - USER_PRESET_ID_START;
    }
    return 255;
}

/**
 * @brief Convert slot index to user preset ID
 */
uint8_t slotToUserId(uint8_t slot) {
    return USER_PRESET_ID_START + slot;
}

/**
 * @brief Serialize zone segment to JSON
 */
void serializeZoneSegment(JsonObject& obj, const zones::ZoneSegment& seg) {
    obj["s1LeftStart"] = seg.s1LeftStart;
    obj["s1LeftEnd"] = seg.s1LeftEnd;
    obj["s1RightStart"] = seg.s1RightStart;
    obj["s1RightEnd"] = seg.s1RightEnd;
}

/**
 * @brief Serialize zone config to JSON zone array
 */
void serializeZones(JsonArray& zonesArray, const persistence::ZoneConfigData& config) {
    for (uint8_t i = 0; i < config.zoneCount; i++) {
        JsonObject zone = zonesArray.add<JsonObject>();
        zone["effectId"] = config.zoneEffects[i];
        zone["paletteId"] = config.zonePalette[i];
        zone["brightness"] = config.zoneBrightness[i];
        zone["speed"] = config.zoneSpeed[i];
        zone["blendMode"] = config.zoneBlendMode[i];
        zone["enabled"] = config.zoneEnabled[i];

        JsonObject segments = zone["segments"].to<JsonObject>();
        serializeZoneSegment(segments, config.segments[i]);
    }
}

} // anonymous namespace

// ============================================================================
// Handler Implementations
// ============================================================================

void ZonePresetHandlers::handleList(AsyncWebServerRequest* request) {
    sendSuccessResponse(request, [](JsonObject& data) {
        JsonArray presets = data["presets"].to<JsonArray>();
        uint16_t totalCount = 0;

        // Add built-in presets (IDs 0-4)
        for (uint8_t i = 0; i < BUILTIN_PRESET_COUNT; i++) {
            JsonObject preset = presets.add<JsonObject>();
            preset["id"] = i;
            preset["name"] = persistence::ZoneConfigManager::getPresetName(i);
            preset["zoneCount"] = persistence::ZONE_PRESETS[i].config.zoneCount;
            preset["builtin"] = true;
            preset["timestamp"] = 0; // Built-in presets have no timestamp
            totalCount++;
        }

        // Add user presets (IDs 10-19)
        UserZonePreset userPreset;
        for (uint8_t slot = 0; slot < USER_PRESET_MAX_COUNT; slot++) {
            if (loadUserPreset(slot, userPreset)) {
                JsonObject preset = presets.add<JsonObject>();
                preset["id"] = slotToUserId(slot);
                preset["name"] = userPreset.name;
                preset["zoneCount"] = userPreset.config.zoneCount;
                preset["builtin"] = false;
                preset["timestamp"] = userPreset.timestamp;
                totalCount++;
            }
        }

        data["count"] = totalCount;
        data["builtinCount"] = BUILTIN_PRESET_COUNT;
        data["maxUserPresets"] = USER_PRESET_MAX_COUNT;
    });
}

void ZonePresetHandlers::handleGet(AsyncWebServerRequest* request, uint8_t id) {
    // Check if built-in preset
    if (isBuiltinPreset(id)) {
        if (id >= BUILTIN_PRESET_COUNT) {
            sendErrorResponse(request, HttpStatus::NOT_FOUND,
                              ErrorCodes::NOT_FOUND, "Preset not found");
            return;
        }

        const persistence::ZonePreset& preset = persistence::ZONE_PRESETS[id];

        // Create a config with segments filled in (same logic as ZoneConfigManager::loadPreset)
        persistence::ZoneConfigData config = preset.config;
        if (config.zoneCount == 1) {
            memcpy(config.segments, zones::ZONE_1_CONFIG, sizeof(zones::ZONE_1_CONFIG));
        } else if (config.zoneCount == 2) {
            memcpy(config.segments, zones::ZONE_2_CONFIG, sizeof(zones::ZONE_2_CONFIG));
        } else if (config.zoneCount == 4) {
            memcpy(config.segments, zones::ZONE_4_CONFIG, sizeof(zones::ZONE_4_CONFIG));
        } else {
            memcpy(config.segments, zones::ZONE_3_CONFIG, sizeof(zones::ZONE_3_CONFIG));
            config.zoneCount = 3;
        }

        sendSuccessResponse(request, [id, &preset, &config](JsonObject& data) {
            data["id"] = id;
            data["name"] = preset.name;
            data["zoneCount"] = config.zoneCount;
            data["builtin"] = true;
            data["timestamp"] = 0;

            JsonArray zones = data["zones"].to<JsonArray>();
            serializeZones(zones, config);
        });
        return;
    }

    // Check if user preset
    uint8_t slot = userIdToSlot(id);
    if (slot == 255) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Invalid preset ID");
        return;
    }

    UserZonePreset userPreset;
    if (!loadUserPreset(slot, userPreset)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found");
        return;
    }

    sendSuccessResponse(request, [id, &userPreset](JsonObject& data) {
        data["id"] = id;
        data["name"] = userPreset.name;
        data["zoneCount"] = userPreset.config.zoneCount;
        data["builtin"] = false;
        data["timestamp"] = userPreset.timestamp;

        JsonArray zones = data["zones"].to<JsonArray>();
        serializeZones(zones, userPreset.config);
    });
}

void ZonePresetHandlers::handleSave(AsyncWebServerRequest* request, uint8_t* data, size_t len, zones::ZoneComposer* composer) {
    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    // Parse request body for preset name
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_JSON, "Invalid JSON body");
        return;
    }

    const char* name = doc["name"] | "";
    if (strlen(name) == 0 || strlen(name) > 31) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Name must be 1-31 characters", "name");
        return;
    }

    // Find next available slot
    uint8_t slot = findNextUserSlot();
    if (slot == 255) {
        sendErrorResponse(request, HttpStatus::INSUFFICIENT_STORAGE,
                          ErrorCodes::STORAGE_FULL, "No more preset slots available");
        return;
    }

    // Create user preset from current ZoneComposer state
    UserZonePreset preset;
    memset(&preset, 0, sizeof(preset));
    strncpy(preset.name, name, sizeof(preset.name) - 1);
    preset.timestamp = millis() / 1000; // Seconds since boot (as proxy for timestamp)

    // Export current zone configuration
    preset.config.version = 2;
    preset.config.systemEnabled = composer->isEnabled();
    preset.config.zoneCount = composer->getZoneCount();

    // Copy zone segments
    const zones::ZoneSegment* segments = composer->getZoneConfig();
    memcpy(preset.config.segments, segments, preset.config.zoneCount * sizeof(zones::ZoneSegment));

    // Copy per-zone settings
    for (uint8_t i = 0; i < zones::MAX_ZONES; i++) {
        preset.config.zoneEffects[i] = composer->getZoneEffect(i);
        preset.config.zoneEnabled[i] = composer->isZoneEnabled(i);
        preset.config.zoneBrightness[i] = composer->getZoneBrightness(i);
        preset.config.zoneSpeed[i] = composer->getZoneSpeed(i);
        preset.config.zonePalette[i] = composer->getZonePalette(i);
        preset.config.zoneBlendMode[i] = static_cast<uint8_t>(composer->getZoneBlendMode(i));
    }

    // Save to NVS
    if (!saveUserPreset(slot, preset)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to save preset to NVS");
        return;
    }

    uint8_t presetId = slotToUserId(slot);
    sendSuccessResponse(request, [presetId, &preset](JsonObject& respData) {
        respData["id"] = presetId;
        respData["name"] = preset.name;
        respData["zoneCount"] = preset.config.zoneCount;
        respData["message"] = "Preset saved successfully";
    }, HttpStatus::CREATED);
}

void ZonePresetHandlers::handleApply(AsyncWebServerRequest* request, uint8_t id, actors::ActorSystem& orchestrator, zones::ZoneComposer* composer, std::function<void()> broadcastFn) {
    (void)orchestrator; // May be used for future actor-based notifications

    if (!composer) {
        sendErrorResponse(request, HttpStatus::SERVICE_UNAVAILABLE,
                          ErrorCodes::FEATURE_DISABLED, "Zone system not available");
        return;
    }

    // Check if built-in preset
    if (isBuiltinPreset(id)) {
        // Use ZoneConfigManager to load built-in preset
        // We need to create a temporary ZoneConfigManager to load the preset
        persistence::ZoneConfigManager configMgr(composer);
        if (!configMgr.loadPreset(id)) {
            sendErrorResponse(request, HttpStatus::NOT_FOUND,
                              ErrorCodes::NOT_FOUND, "Preset not found or invalid");
            return;
        }

        // Broadcast zone state change
        if (broadcastFn) {
            broadcastFn();
        }

        sendSuccessResponse(request, [id](JsonObject& respData) {
            respData["id"] = id;
            respData["name"] = persistence::ZoneConfigManager::getPresetName(id);
            respData["applied"] = true;
        });
        return;
    }

    // Check if user preset
    uint8_t slot = userIdToSlot(id);
    if (slot == 255) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Invalid preset ID");
        return;
    }

    UserZonePreset userPreset;
    if (!loadUserPreset(slot, userPreset)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found");
        return;
    }

    // Apply user preset configuration to ZoneComposer
    // Set layout first (affects zone count)
    if (!composer->setLayout(userPreset.config.segments, userPreset.config.zoneCount)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to set zone layout from preset");
        return;
    }

    // Apply per-zone settings
    for (uint8_t i = 0; i < zones::MAX_ZONES; i++) {
        composer->setZoneEffect(i, userPreset.config.zoneEffects[i]);
        composer->setZoneEnabled(i, userPreset.config.zoneEnabled[i]);
        composer->setZoneBrightness(i, userPreset.config.zoneBrightness[i]);
        composer->setZoneSpeed(i, userPreset.config.zoneSpeed[i]);
        composer->setZonePalette(i, userPreset.config.zonePalette[i]);
        composer->setZoneBlendMode(i, static_cast<zones::BlendMode>(userPreset.config.zoneBlendMode[i]));
    }

    // Apply system enabled state
    composer->setEnabled(userPreset.config.systemEnabled);

    // Broadcast zone state change
    if (broadcastFn) {
        broadcastFn();
    }

    sendSuccessResponse(request, [id, &userPreset](JsonObject& respData) {
        respData["id"] = id;
        respData["name"] = userPreset.name;
        respData["applied"] = true;
    });
}

void ZonePresetHandlers::handleDelete(AsyncWebServerRequest* request, uint8_t id) {
    // Cannot delete built-in presets
    if (isBuiltinPreset(id)) {
        sendErrorResponse(request, HttpStatus::FORBIDDEN,
                          ErrorCodes::INVALID_ACTION, "Cannot delete built-in presets");
        return;
    }

    // Check if user preset
    uint8_t slot = userIdToSlot(id);
    if (slot == 255) {
        sendErrorResponse(request, HttpStatus::BAD_REQUEST,
                          ErrorCodes::INVALID_VALUE, "Invalid preset ID");
        return;
    }

    // Check if preset exists
    UserZonePreset userPreset;
    if (!loadUserPreset(slot, userPreset)) {
        sendErrorResponse(request, HttpStatus::NOT_FOUND,
                          ErrorCodes::NOT_FOUND, "Preset not found");
        return;
    }

    // Delete the preset
    if (!deleteUserPreset(slot)) {
        sendErrorResponse(request, HttpStatus::INTERNAL_ERROR,
                          ErrorCodes::INTERNAL_ERROR, "Failed to delete preset from NVS");
        return;
    }

    sendSuccessResponse(request, [id](JsonObject& respData) {
        respData["id"] = id;
        respData["deleted"] = true;
    });
}

} // namespace handlers
} // namespace webserver
} // namespace network
} // namespace lightwaveos
