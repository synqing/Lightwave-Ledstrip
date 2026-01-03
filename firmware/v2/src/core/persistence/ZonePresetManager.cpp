/**
 * @file ZonePresetManager.cpp
 * @brief Zone preset persistence implementation
 */

#include "ZonePreset.h"
#include "../../effects/zones/ZoneComposer.h"
#include "../../effects/zones/ZoneDefinition.h"
#include "../../effects/zones/BlendMode.h"
#include "../actors/ActorSystem.h"

#include <cstring>
#include <Arduino.h>

namespace lightwaveos {
namespace persistence {

using namespace lightwaveos::zones;

// ==================== ZonePreset Implementation ====================

void ZonePreset::calculateChecksum() {
    const size_t dataSize = offsetof(ZonePreset, checksum);
    checksum = NVSManager::calculateCRC32(this, dataSize);
}

bool ZonePreset::isValid() const {
    if (version != CURRENT_VERSION) return false;
    const size_t dataSize = offsetof(ZonePreset, checksum);
    uint32_t calculated = NVSManager::calculateCRC32(this, dataSize);
    return (checksum == calculated);
}

void ZonePreset::clamp() {
    // Clamp zone count
    if (zoneCount < 1) zoneCount = 1;
    if (zoneCount > MAX_ZONES) zoneCount = MAX_ZONES;

    // Clamp per-zone values
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        // effectId: 0-96 (will be validated at apply time)
        if (zones[i].effectId > 96) zones[i].effectId = 0;

        // brightness: 0-255, no clamping needed

        // speed: 1-100
        if (zones[i].speed < 1) zones[i].speed = 1;
        if (zones[i].speed > 100) zones[i].speed = 100;

        // paletteId: 0-56
        if (zones[i].paletteId > 56) zones[i].paletteId = 0;

        // blendMode: 0-7
        if (zones[i].blendMode > 7) zones[i].blendMode = 0;
    }

    // Clamp segment values to strip length (0-159)
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
        if (segments[i].s1LeftStart > 159) segments[i].s1LeftStart = 0;
        if (segments[i].s1LeftEnd > 159) segments[i].s1LeftEnd = 79;
        if (segments[i].s1RightStart > 159) segments[i].s1RightStart = 80;
        if (segments[i].s1RightEnd > 159) segments[i].s1RightEnd = 159;
    }
}

// ==================== ZonePresetManager Implementation ====================

ZonePresetManager& ZonePresetManager::instance() {
    static ZonePresetManager inst;
    return inst;
}

void ZonePresetManager::makeKey(uint8_t id, char* key) {
    snprintf(key, 16, "zpreset_%u", id);
}

void ZonePresetManager::captureFromComposer(ZonePreset& preset, const ZoneComposer* composer) {
    if (!composer) return;

    // Capture enabled state and zone count
    preset.zonesEnabled = composer->isEnabled();
    preset.zoneCount = composer->getZoneCount();

    // Capture per-zone configuration
    for (uint8_t i = 0; i < ZonePreset::MAX_ZONES; i++) {
        preset.zones[i].enabled = composer->isZoneEnabled(i);
        preset.zones[i].effectId = composer->getZoneEffect(i);
        preset.zones[i].brightness = composer->getZoneBrightness(i);
        preset.zones[i].speed = composer->getZoneSpeed(i);
        preset.zones[i].paletteId = composer->getZonePalette(i);
        preset.zones[i].blendMode = static_cast<uint8_t>(composer->getZoneBlendMode(i));
    }

    // Capture segment layout
    const ZoneSegment* segments = composer->getZoneConfig();
    if (segments) {
        for (uint8_t i = 0; i < preset.zoneCount && i < ZonePreset::MAX_ZONES; i++) {
            preset.segments[i].s1LeftStart = segments[i].s1LeftStart;
            preset.segments[i].s1LeftEnd = segments[i].s1LeftEnd;
            preset.segments[i].s1RightStart = segments[i].s1RightStart;
            preset.segments[i].s1RightEnd = segments[i].s1RightEnd;
        }
    }
}

int8_t ZonePresetManager::savePreset(const char* name, const ZoneComposer* composer) {
    // Find a free slot
    int8_t slot = findFreeSlot();
    if (slot < 0) {
        Serial.println("[ZonePreset] ERROR: No free preset slots");
        return -1;
    }

    if (savePresetAt(slot, name, composer)) {
        return slot;
    }
    return -1;
}

bool ZonePresetManager::savePresetAt(uint8_t id, const char* name, const ZoneComposer* composer) {
    if (id >= MAX_PRESETS) {
        Serial.println("[ZonePreset] ERROR: Invalid preset ID");
        return false;
    }

    if (!composer) {
        Serial.println("[ZonePreset] ERROR: ZoneComposer not available");
        return false;
    }

    // Build preset
    ZonePreset preset;
    preset.version = ZonePreset::CURRENT_VERSION;
    strncpy(preset.name, name ? name : "Unnamed", ZonePreset::NAME_MAX_LEN - 1);
    preset.name[ZonePreset::NAME_MAX_LEN - 1] = '\0';

    // Capture current state from composer
    captureFromComposer(preset, composer);

    preset.clamp();
    preset.calculateChecksum();

    // Save to NVS
    char key[16];
    makeKey(id, key);
    NVSResult result = NVS_MANAGER.saveBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));

    if (result == NVSResult::OK) {
        Serial.printf("[ZonePreset] Preset '%s' saved to slot %d (zones=%d, enabled=%s)\n",
                      preset.name, id, preset.zoneCount, preset.zonesEnabled ? "true" : "false");
        return true;
    } else {
        Serial.printf("[ZonePreset] ERROR: Save failed: %s\n",
                      NVSManager::resultToString(result));
        return false;
    }
}

bool ZonePresetManager::applyPreset(uint8_t id, ZoneComposer* composer, actors::ActorSystem& actorSystem) {
    if (!composer) {
        Serial.println("[ZonePreset] ERROR: ZoneComposer not available");
        return false;
    }

    ZonePreset preset;
    if (!getPreset(id, preset)) {
        return false;
    }

    // Apply zone count and layout
    // Determine which layout to use based on zone count
    const ZoneSegment* layoutConfig = nullptr;
    if (preset.zoneCount == 4) {
        layoutConfig = ZONE_4_CONFIG;
    } else {
        layoutConfig = ZONE_3_CONFIG;
    }
    composer->setLayout(layoutConfig, preset.zoneCount);

    // Apply per-zone settings
    for (uint8_t i = 0; i < preset.zoneCount && i < ZonePreset::MAX_ZONES; i++) {
        composer->setZoneEffect(i, preset.zones[i].effectId);
        composer->setZoneBrightness(i, preset.zones[i].brightness);
        composer->setZoneSpeed(i, preset.zones[i].speed);
        composer->setZonePalette(i, preset.zones[i].paletteId);
        composer->setZoneBlendMode(i, static_cast<BlendMode>(preset.zones[i].blendMode));
        composer->setZoneEnabled(i, preset.zones[i].enabled);
    }

    // Apply enabled state
    composer->setEnabled(preset.zonesEnabled);

    Serial.printf("[ZonePreset] Preset '%s' applied from slot %d\n", preset.name, id);
    return true;
}

bool ZonePresetManager::getPreset(uint8_t id, ZonePreset& preset) {
    if (id >= MAX_PRESETS) return false;

    char key[16];
    makeKey(id, key);

    NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));

    if (result != NVSResult::OK) {
        return false;
    }

    if (!preset.isValid()) {
        Serial.printf("[ZonePreset] WARNING: Preset %d has invalid checksum\n", id);
        return false;
    }

    // Clamp values for safety
    preset.clamp();
    return true;
}

bool ZonePresetManager::deletePreset(uint8_t id) {
    if (id >= MAX_PRESETS) return false;

    char key[16];
    makeKey(id, key);

    NVSResult result = NVS_MANAGER.eraseKey(NVS_NAMESPACE, key);

    if (result == NVSResult::OK || result == NVSResult::NOT_FOUND) {
        Serial.printf("[ZonePreset] Preset %d deleted\n", id);
        return true;
    }

    Serial.printf("[ZonePreset] ERROR: Delete failed: %s\n",
                  NVSManager::resultToString(result));
    return false;
}

uint8_t ZonePresetManager::listPresets(char names[][ZonePreset::NAME_MAX_LEN], uint8_t* ids) {
    uint8_t count = 0;

    for (uint8_t i = 0; i < MAX_PRESETS && count < MAX_PRESETS; i++) {
        ZonePreset preset;
        char key[16];
        makeKey(i, key);

        NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));

        if (result == NVSResult::OK && preset.isValid()) {
            if (names) {
                strncpy(names[count], preset.name, ZonePreset::NAME_MAX_LEN);
            }
            if (ids) {
                ids[count] = i;
            }
            count++;
        }
    }

    return count;
}

bool ZonePresetManager::hasPreset(uint8_t id) const {
    if (id >= MAX_PRESETS) return false;

    ZonePreset preset;
    char key[16];
    makeKey(id, key);

    NVSResult result = NVS_MANAGER.loadBlob(NVS_NAMESPACE, key, &preset, sizeof(preset));
    return (result == NVSResult::OK && preset.isValid());
}

uint8_t ZonePresetManager::getPresetCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        if (hasPreset(i)) count++;
    }
    return count;
}

int8_t ZonePresetManager::findFreeSlot() const {
    for (uint8_t i = 0; i < MAX_PRESETS; i++) {
        if (!hasPreset(i)) return i;
    }
    return -1;
}

} // namespace persistence
} // namespace lightwaveos
