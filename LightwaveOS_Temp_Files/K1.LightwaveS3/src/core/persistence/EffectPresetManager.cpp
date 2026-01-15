/**
 * @file EffectPresetManager.cpp
 * @brief Effect preset persistence implementation
 */

#include "EffectPreset.h"

#include <cstring>
#include <Arduino.h>

namespace lightwaveos {
namespace persistence {

// ==================== EffectPreset Implementation ====================

void EffectPreset::calculateChecksum() {
    const size_t dataSize = offsetof(EffectPreset, checksum);
    checksum = NVSManager::calculateCRC32(this, dataSize);
}

bool EffectPreset::isValid() const {
    if (version != CURRENT_VERSION) return false;
    const size_t dataSize = offsetof(EffectPreset, checksum);
    uint32_t calculated = NVSManager::calculateCRC32(this, dataSize);
    return (checksum == calculated);
}

void EffectPreset::clamp() {
    // Clamp to valid ranges
    if (effectId > 96) effectId = 0;
    // brightness is 0-255, no clamping needed
    if (speed < 1) speed = 1;
    if (speed > 50) speed = 50;
    if (paletteId > 36) paletteId = 0;
}

// ==================== EffectPresetManager Implementation ====================

EffectPresetManager& EffectPresetManager::instance() {
    static EffectPresetManager inst;
    return inst;
}

void EffectPresetManager::makeKey(uint8_t id, char* key) {
    snprintf(key, 16, "preset_%u", id);
}

int8_t EffectPresetManager::savePreset(const char* name,
                                        uint8_t effectId,
                                        uint8_t brightness,
                                        uint8_t speed,
                                        uint8_t paletteId) {
    // Find a free slot
    int8_t slot = findFreeSlot();
    if (slot < 0) {
        Serial.println("[EffectPreset] ERROR: No free preset slots");
        return -1;
    }

    if (savePresetAt(slot, name, effectId, brightness, speed, paletteId)) {
        return slot;
    }
    return -1;
}

bool EffectPresetManager::savePresetAt(uint8_t id,
                                        const char* name,
                                        uint8_t effectId,
                                        uint8_t brightness,
                                        uint8_t speed,
                                        uint8_t paletteId) {
    if (id >= MAX_PRESETS) {
        Serial.println("[EffectPreset] ERROR: Invalid preset ID");
        return false;
    }

    // Build preset
    EffectPreset preset;
    preset.version = EffectPreset::CURRENT_VERSION;
    strncpy(preset.name, name ? name : "Unnamed", EffectPreset::NAME_MAX_LEN - 1);
    preset.name[EffectPreset::NAME_MAX_LEN - 1] = '\0';
    preset.effectId = effectId;
    preset.brightness = brightness;
    preset.speed = speed;
    preset.paletteId = paletteId;
    preset.clamp();
    preset.calculateChecksum();

    // Save to NVS
    char key[16];
    makeKey(id, key);
    NVSResult result = NVS_MANAGER.saveBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));

    if (result == NVSResult::OK) {
        Serial.printf("[EffectPreset] Preset '%s' saved to slot %d (effect=%d, brightness=%d, speed=%d, palette=%d)\n",
                      preset.name, id, effectId, brightness, speed, paletteId);
        return true;
    } else {
        Serial.printf("[EffectPreset] ERROR: Save failed: %s\n",
                      NVSManager::resultToString(result));
        return false;
    }
}

bool EffectPresetManager::loadPreset(uint8_t id,
                                      uint8_t& effectId,
                                      uint8_t& brightness,
                                      uint8_t& speed,
                                      uint8_t& paletteId,
                                      char* nameOut) {
    EffectPreset preset;
    if (!getPreset(id, preset)) {
        return false;
    }

    // Apply values (already clamped in getPreset)
    effectId = preset.effectId;
    brightness = preset.brightness;
    speed = preset.speed;
    paletteId = preset.paletteId;

    if (nameOut) {
        strncpy(nameOut, preset.name, EffectPreset::NAME_MAX_LEN - 1);
        nameOut[EffectPreset::NAME_MAX_LEN - 1] = '\0';
    }

    Serial.printf("[EffectPreset] Preset '%s' loaded from slot %d\n", preset.name, id);
    return true;
}

bool EffectPresetManager::getPreset(uint8_t id, EffectPreset& preset) {
    if (id >= MAX_PRESETS) return false;

    char key[16];
    makeKey(id, key);

    NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));

    if (result != NVSResult::OK) {
        return false;
    }

    if (!preset.isValid()) {
        Serial.printf("[EffectPreset] WARNING: Preset %d has invalid checksum\n", id);
        return false;
    }

    // Clamp values for safety
    preset.clamp();
    return true;
}

bool EffectPresetManager::deletePreset(uint8_t id) {
    if (id >= MAX_PRESETS) return false;

    char key[16];
    makeKey(id, key);

    NVSResult result = NVS_MANAGER.eraseKey(NVS_NAMESPACE, key);

    if (result == NVSResult::OK || result == NVSResult::NOT_FOUND) {
        Serial.printf("[EffectPreset] Preset %d deleted\n", id);
        return true;
    }

    Serial.printf("[EffectPreset] ERROR: Delete failed: %s\n",
                  NVSManager::resultToString(result));
    return false;
}

uint8_t EffectPresetManager::listPresets(char names[][EffectPreset::NAME_MAX_LEN], uint8_t* ids) {
    uint8_t count = 0;

    for (uint8_t i = 0; i < MAX_PRESETS && count < MAX_PRESETS; i++) {
        EffectPreset preset;
        char key[16];
        makeKey(i, key);

        NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));

        if (result == NVSResult::OK && preset.isValid()) {
            if (names) {
                strncpy(names[count], preset.name, EffectPreset::NAME_MAX_LEN - 1);
                names[count][EffectPreset::NAME_MAX_LEN - 1] = '\0';
            }
            if (ids) {
                ids[count] = i;
            }
            count++;
        }
    }

    return count;
}

bool EffectPresetManager::hasPreset(uint8_t id) const {
    if (id >= MAX_PRESETS) return false;

    EffectPreset preset;
    char key[16];
    makeKey(id, key);

    NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));
    return (result == NVSResult::OK && preset.isValid());
}

uint8_t EffectPresetManager::getPresetCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        if (hasPreset(i)) count++;
    }
    return count;
}

int8_t EffectPresetManager::findFreeSlot() const {
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        if (!hasPreset(i)) return i;
    }
    return -1;
}

} // namespace persistence
} // namespace lightwaveos
